# frozen_string_literal: true

module TiDatabaseGenerator
  FIXED_CLASS_ENUMERATION_NAMES = {
    "::Object" => "TI_CLASS_OBJECT",
    "::Integer" => "TI_CLASS_INTEGER",
    "::Float" => "TI_CLASS_FLOAT",
    "::String" => "TI_CLASS_STRING",
    "::Symbol" => "TI_CLASS_SYMBOL",
    "::Array" => "TI_CLASS_ARRAY",
    "::Hash" => "TI_CLASS_HASH",
    "::Range" => "TI_CLASS_RANGE",
    "::Proc" => "TI_CLASS_PROC",
    "::TrueClass" => "TI_CLASS_TRUE",
    "::FalseClass" => "TI_CLASS_FALSE",
    "::NilClass" => "TI_CLASS_NIL",
    "::Untyped" => "TI_CLASS_UNTYPED",
    "::Class" => "TI_CLASS_CLASS",
    "::Kernel" => "TI_CLASS_KERNEL"
  }.freeze

  class DatabaseBuilder
    def build(collected_declarations:)
      @collected_classes = collected_declarations.collected_classes

      validate_required_fixed_classes_present!
      order_class_names
      assign_class_identifiers_and_enumeration_names

      @type_resolver =
        TypeResolver.new(
          type_aliases: collected_declarations.type_aliases,
          class_identifiers_by_full_name: @class_identifiers_by_full_name
        )

      @signature_renderer = SignatureRenderer.new
      @name_pool = StringPool.new(pool_name: "name")
      @signature_pool = StringPool.new(pool_name: "signature")
      @document_pool = StringPool.new(pool_name: "document")

      @union_pool = UnionPool.new

      build_builtin_tables

      BuiltinDatabase.new(
        builtin_classes: @builtin_classes,
        builtin_methods: @builtin_methods,
        builtin_arguments: @builtin_arguments,
        class_identifiers_by_full_name: @class_identifiers_by_full_name,
        enumeration_names: @enumeration_names,
        name_pool: @name_pool,
        signature_pool: @signature_pool,
        document_pool: @document_pool,
        union_pool: @union_pool
      )
    end

    private

    def validate_required_fixed_classes_present!
      fixed_class_names_missing_from_collected_classes =
        FIXED_CLASS_ENUMERATION_NAMES.keys.reject do |fixed_class_name|
          @collected_classes.key?(fixed_class_name)
        end

      unless fixed_class_names_missing_from_collected_classes.empty?
        raise "required RBS declarations are missing: #{fixed_class_names_missing_from_collected_classes.join(', ')}"
      end
    end

    def order_class_names
      fixed_class_names = FIXED_CLASS_ENUMERATION_NAMES.keys
      collected_class_names_excluding_fixed = @collected_classes.keys - fixed_class_names

      @ordered_class_names = fixed_class_names + collected_class_names_excluding_fixed.sort
      if @ordered_class_names.length >= 255
        raise "class ID exceeds uint8_t: #{@ordered_class_names.length}"
      end
    end

    def assign_class_identifiers_and_enumeration_names
      @class_identifiers_by_full_name = {}
      @enumeration_names = { 0 => "TI_CLASS_NONE" }

      @ordered_class_names.each_with_index do |full_class_name, class_name_index|
        class_identifier = class_name_index + 1
        @class_identifiers_by_full_name[full_class_name] = class_identifier

        enumeration_name = FIXED_CLASS_ENUMERATION_NAMES[full_class_name]
        unless enumeration_name
          enumeration_name = build_enumeration_name(full_class_name:)
        end

        @enumeration_names[class_identifier] = enumeration_name
      end
    end

    def build_enumeration_name(full_class_name:)
      normalized_class_name = full_class_name.delete_prefix("::").gsub("::", "_")
      normalized_class_name = normalized_class_name.gsub(/([a-z])([A-Z])/, '\\1_\\2')
      normalized_class_name = normalized_class_name.upcase.gsub(/[^A-Z0-9_]/, "_")
      "TI_CLASS_#{normalized_class_name}"
    end

    def build_builtin_tables
      @builtin_classes = Array.new(@class_identifiers_by_full_name.length + 1)
      @builtin_classes[0] = build_empty_builtin_class

      @builtin_methods = []
      @builtin_arguments = []

      @ordered_class_names.each do |full_class_name|
        class_identifier = @class_identifiers_by_full_name[full_class_name]
        collected_class = @collected_classes[full_class_name]

        builtin_class = build_empty_builtin_class

        builtin_class.name_offset =
          @name_pool.add_string_and_return_offset(
            string: full_class_name.delete_prefix("::")
          )

        append_builtin_methods_and_set_class_method_range(
          collected_class:,
          use_static_methods: false,
          builtin_class:
        )

        append_builtin_methods_and_set_class_method_range(
          collected_class:,
          use_static_methods: true,
          builtin_class:
        )

        @builtin_classes[class_identifier] = builtin_class
      end

      if @builtin_methods.length > 65_535
        raise "method table exceeds 65535 entries (#{@builtin_methods.length})"
      end

      if @builtin_arguments.length > 65_535
        raise "argument table exceeds 65535 entries (#{@builtin_arguments.length})"
      end
    end

    def build_empty_builtin_class
      BuiltinClassRecord.new(
        name_offset: 0,
        instance_method_start_index: 0,
        instance_method_count: 0,
        static_method_start_index: 0,
        static_method_count: 0
      )
    end

    def append_builtin_methods_and_set_class_method_range(
      collected_class:,
      use_static_methods:,
      builtin_class:
    )
      resolved_collected_methods =
        resolve_collected_methods_with_object_methods(
          collected_class:,
          use_static_methods:
        )

      start_index_field_name = :instance_method_start_index
      count_field_name = :instance_method_count

      if use_static_methods
        start_index_field_name = :static_method_start_index
        count_field_name = :static_method_count
      end

      builtin_class[start_index_field_name] = @builtin_methods.length

      resolved_collected_methods.each do |resolved_collected_method|
        @builtin_methods << build_builtin_method_record(
          collected_class:,
          resolved_collected_method:
        )
      end

      builtin_class[count_field_name] =
        resolved_collected_methods.length
    end

    def resolve_collected_methods_with_object_methods(
      collected_class:,
      use_static_methods:
    )
      resolved_collected_methods_by_name = {}
      visited_class_full_names = {}

      collect_methods_from_class_and_eligible_ancestors(
        current_collected_class: collected_class,
        owner_collected_class: collected_class,
        use_static_methods:,
        substitution: RBS::Substitution.new,
        resolved_collected_methods_by_name:,
        visited_class_full_names:
      )

      collected_object_class = @collected_classes["::Object"]

      if collected_class.full_name != "::Object" && collected_object_class
        collect_methods_from_class_and_eligible_ancestors(
          current_collected_class: collected_object_class,
          owner_collected_class: collected_class,
          use_static_methods:,
          substitution: RBS::Substitution.new,
          resolved_collected_methods_by_name:,
          visited_class_full_names:
        )
      end

      # sort by name asc
      resolved_collected_methods_by_name.keys.sort.map do |method_name|
        resolved_collected_methods_by_name[method_name]
      end
    end

    def collect_methods_from_class_and_eligible_ancestors(
      current_collected_class:,
      owner_collected_class:,
      use_static_methods:,
      substitution:,
      resolved_collected_methods_by_name:,
      visited_class_full_names:
    )
      return if visited_class_full_names[current_collected_class.full_name]

      if current_collected_class.full_name == "::Kernel" &&
          current_collected_class != owner_collected_class

        return
      end

      visited_class_full_names[current_collected_class.full_name] = true

      method_table =
        if use_static_methods
          current_collected_class.static_methods
        else
          current_collected_class.instance_methods
        end

      method_table.each do |method_name, collected_method|
        unless resolved_collected_methods_by_name.key?(method_name)
          resolved_collected_methods_by_name[method_name] =
            resolve_collected_method(
              collected_method:,
              owner_full_name: owner_collected_class.full_name,
              substitution:
            )
        end
      end

      current_collected_class.direct_ancestors.each do |ancestor|
        ancestor_kind = ancestor[:kind]
        full_ancestor_name = ancestor[:full_name]
        type_arguments = ancestor[:type_arguments]

        if should_collect_ancestor?(ancestor_kind:, use_static_methods:)
          collected_ancestor_class = @collected_classes[full_ancestor_name]

          if collected_ancestor_class
            ancestor_substitution =
              build_ancestor_substitution(
                collected_ancestor_class:,
                type_arguments:,
                substitution:
              )

            collect_methods_from_class_and_eligible_ancestors(
              current_collected_class: collected_ancestor_class,
              owner_collected_class:,
              use_static_methods:,
              substitution: ancestor_substitution,
              resolved_collected_methods_by_name:,
              visited_class_full_names:
            )
          end
        end
      end
    end

    def should_collect_ancestor?(ancestor_kind:, use_static_methods:)
      return ancestor_kind != :include if use_static_methods

      ancestor_kind != :extend
    end

    def build_ancestor_substitution(
      collected_ancestor_class:,
      type_arguments:,
      substitution:
    )
      type_parameter_names =
        collected_ancestor_class.declarations.first.type_params.map(&:name)
      resolved_type_arguments = type_arguments.map do |type_argument|
        type_argument.sub(substitution)
      end
      RBS::Substitution.build(
        type_parameter_names,
        resolved_type_arguments
      )
    end

    def resolve_collected_method(
      collected_method:,
      owner_full_name:,
      substitution:
    )

      ResolvedCollectedMethod.new(
        collected_method:,
        resolved_return_type:
          resolve_method_return_type(
            collected_method:,
            owner_full_name:,
            substitution:
          ),
        resolved_arguments:
          resolve_method_arguments(
            collected_method:,
            owner_full_name:,
            substitution:
          ),
        block_parameter_class_identifier:
          resolve_block_parameter_class_identifier(
            collected_method:,
            owner_full_name:,
            substitution:
          )
      )
    end

    def build_builtin_method_record(
      collected_class:,
      resolved_collected_method:
    )

      collected_method = resolved_collected_method.collected_method
      resolved_return_type = resolved_collected_method.resolved_return_type

      error_context = "#{collected_class.full_name}##{collected_method.name}"

      argument_start_index = @builtin_arguments.length

      append_builtin_arguments(
        resolved_arguments: resolved_collected_method.resolved_arguments,
        error_context:
      )

      argument_count = @builtin_arguments.length - argument_start_index

      if argument_count > 255
        raise "#{error_context}: argument count exceeds 255"
      end

      return_class_identifiers =
        fallback_to_untyped_if_oversized_union(
          class_identifiers: resolved_return_type.class_identifiers,
          error_context: "#{error_context} return type"
        )

      array_variant_class_identifiers =
        fallback_to_untyped_if_oversized_union(
          class_identifiers: resolved_return_type.array_variant_class_identifiers,
          error_context: "#{error_context} array variant"
        )

      BuiltinMethodRecord.new(
        name_offset: @name_pool.add_string_and_return_offset(string: collected_method.name),
        signature_offset: @signature_pool.add_string_and_return_offset(
          string: @signature_renderer.render_method_signature(collected_method:)
        ),
        document_offset: @document_pool.add_string_and_return_offset(
          string: collected_method.comment
        ),
        argument_start_index:,
        argument_count:,
        return_class_identifier: return_class_identifiers.first || 0,
        return_array_variant_class_identifier:
          array_variant_class_identifiers.first || 0,
        return_union_index: @union_pool.resolve_union_index(
          class_identifiers: return_class_identifiers,
          error_context: "#{error_context} return type"
        ),
        array_variant_union_index: @union_pool.resolve_union_index(
          class_identifiers: array_variant_class_identifiers,
          error_context: "#{error_context} array variant"
        ),
        block_parameter_class_identifier:
          resolved_collected_method.block_parameter_class_identifier,
        origin_class_identifier: @class_identifiers_by_full_name.fetch(
          collected_method.origin_class_full_name
        )
      )
    end

    def append_builtin_arguments(resolved_arguments:, error_context:)
      resolved_arguments.each do |resolved_argument|
        append_builtin_argument(
          resolved_argument:,
          error_context:
        )
      end
    end

    def append_builtin_argument(resolved_argument:, error_context:)
      resolved_type = resolved_argument.resolved_type

      class_identifiers =
        fallback_to_untyped_if_oversized_union(
          class_identifiers: resolved_type.class_identifiers,
          error_context: "#{error_context} argument"
        )

      argument_name_offset = 0

      if resolved_argument.name
        argument_name_offset =
          @name_pool.add_string_and_return_offset(
            string: resolved_argument.name
          )
      end

      argument_union_index =
        @union_pool.resolve_union_index(
          class_identifiers:,
          error_context: "#{error_context} argument"
        )

      @builtin_arguments << BuiltinArgumentRecord.new(
        name_offset: argument_name_offset,
        class_identifier: class_identifiers.first || 0,
        union_index: argument_union_index,
        kind: BUILTIN_ARGUMENT_KINDS.fetch(resolved_argument.kind)
      )
    end

    def fallback_to_untyped_if_oversized_union(class_identifiers:, error_context:)
      return class_identifiers unless class_identifiers.length > 4

      warn "#{error_context}: union has #{class_identifiers.length} members, falling back to Untyped"
      [@class_identifiers_by_full_name.fetch("::Untyped")]
    end

    def resolve_method_return_type(
      collected_method:,
      owner_full_name:,
      substitution:
    )
      resolved_return_types =
        collected_method.method_types.map do |method_type|
          @type_resolver.resolve(
            signature_type: method_type.type.return_type.sub(substitution),
            owner_full_name:
          )
        end

      ResolvedType.new(
        class_identifiers:
          resolved_return_types.flat_map(&:class_identifiers).uniq.sort,
        array_variant_class_identifiers:
          resolved_return_types
            .flat_map(&:array_variant_class_identifiers)
            .uniq
            .sort
      )
    end

    def resolve_method_arguments(
      collected_method:,
      owner_full_name:,
      substitution:
    )

      method_types = collected_method.method_types

      resolved_arguments =
        resolve_required_and_optional_positional_arguments(
          method_types:,
          owner_full_name:,
          substitution:
        )

      rest_positional_argument =
        resolve_rest_positional_argument(
          method_types:,
          owner_full_name:,
          substitution:
        )
      resolved_arguments << rest_positional_argument if rest_positional_argument

      resolved_arguments.concat(
        resolve_trailing_positional_arguments(
          method_types:,
          owner_full_name:,
          substitution:
        )
      )

      resolved_arguments.concat(
        resolve_required_and_optional_keyword_arguments(
          method_types:,
          owner_full_name:,
          substitution:
        )
      )

      rest_keyword_argument =
        resolve_rest_keyword_argument(
          method_types:,
          owner_full_name:,
          substitution:
        )
      resolved_arguments << rest_keyword_argument if rest_keyword_argument

      resolved_arguments
    end

    def resolve_required_and_optional_positional_arguments(
      method_types:,
      owner_full_name:,
      substitution:
    )

      maximum_required_and_optional_positional_parameter_count =
        method_types.map do |method_type|
          method_type.type.required_positionals.length +
            method_type.type.optional_positionals.length
        end.max || 0

      (
        0...maximum_required_and_optional_positional_parameter_count
      ).map do |position_index|

        positional_argument_type_parameters_at_index =
          method_types.filter_map do |method_type|
            method_type_required_and_optional_positional_parameters =
              method_type.type.required_positionals +
              method_type.type.optional_positionals

            method_type_required_and_optional_positional_parameters[position_index] ||
              method_type.type.rest_positionals
          end

        is_positional_argument_required_in_all_method_types =
          method_types.all? do |method_type|
            position_index < method_type.type.required_positionals.length
          end

        argument_kind = :optional
        if is_positional_argument_required_in_all_method_types
          argument_kind = :required
        end

        build_resolved_argument(
          argument_name: nil,
          argument_kind:,
          argument_type_parameters: positional_argument_type_parameters_at_index,
          owner_full_name:,
          substitution:
        )
      end
    end

    def resolve_rest_positional_argument(
      method_types:,
      owner_full_name:,
      substitution:
    )

      rest_positional_parameters =
        method_types.filter_map do |method_type|
          method_type.type.rest_positionals
        end

      return if rest_positional_parameters.empty?

      build_resolved_argument(
        argument_name: nil,
        argument_kind: :rest,
        argument_type_parameters: rest_positional_parameters,
        owner_full_name:,
        substitution:
      )
    end

    def resolve_trailing_positional_arguments(
      method_types:,
      owner_full_name:,
      substitution:
    )

      maximum_trailing_positional_parameter_count =
        method_types.map do |method_type|
          method_type.type.trailing_positionals.length
        end.max || 0

      (
        0...maximum_trailing_positional_parameter_count
      ).map do |position_index|

        trailing_positional_argument_type_parameters_at_index =
          method_types.filter_map do |method_type|
            method_type_trailing_positional_parameters =
              method_type.type.trailing_positionals

            method_type_trailing_positional_parameters[position_index]
          end

        is_trailing_positional_argument_present_in_all_method_types =
          method_types.all? do |method_type|
            method_type_trailing_positional_parameters =
              method_type.type.trailing_positionals

            position_index < method_type_trailing_positional_parameters.length
          end

        argument_kind = :optional
        if is_trailing_positional_argument_present_in_all_method_types
          argument_kind = :required
        end

        build_resolved_argument(
          argument_name: nil,
          argument_kind:,
          argument_type_parameters:
            trailing_positional_argument_type_parameters_at_index,
          owner_full_name:,
          substitution:
        )
      end
    end

    def resolve_required_and_optional_keyword_arguments(
      method_types:,
      owner_full_name:,
      substitution:
    )
      keyword_names =
        method_types.flat_map do |method_type|
          method_type.type.required_keywords.keys +
            method_type.type.optional_keywords.keys
        end.uniq.sort

      keyword_names.map do |keyword_name|
        keyword_argument_type_parameters =
          method_types.filter_map do |method_type|
            method_type.type.required_keywords[keyword_name] ||
              method_type.type.optional_keywords[keyword_name] ||
              method_type.type.rest_keywords
          end

        is_keyword_argument_required_in_all_method_types =
          method_types.all? do |method_type|
            method_type.type.required_keywords.key?(keyword_name)
          end

        argument_kind = :optional_keyword
        if is_keyword_argument_required_in_all_method_types
          argument_kind = :required_keyword
        end

        build_resolved_argument(
          argument_name: keyword_name,
          argument_kind:,
          argument_type_parameters: keyword_argument_type_parameters,
          owner_full_name:,
          substitution:
        )
      end
    end

    def resolve_rest_keyword_argument(
      method_types:,
      owner_full_name:,
      substitution:
    )
      rest_keyword_parameters =
        method_types.filter_map do |method_type|
          method_type.type.rest_keywords
        end

      return if rest_keyword_parameters.empty?

      build_resolved_argument(
        argument_name: nil,
        argument_kind: :rest_keyword,
        argument_type_parameters: rest_keyword_parameters,
        owner_full_name:,
        substitution:
      )
    end

    def build_resolved_argument(
      argument_name:,
      argument_kind:,
      argument_type_parameters:,
      owner_full_name:,
      substitution:
    )
      ResolvedArgument.new(
        name: argument_name,
        kind: argument_kind,
        resolved_type:
          resolve_argument_type(
            argument_type_parameters:,
            owner_full_name:,
            substitution:
          )
      )
    end

    def resolve_argument_type(
      argument_type_parameters:,
      owner_full_name:,
      substitution:
    )
      resolved_argument_parameter_types =
        argument_type_parameters.map do |argument_type_parameter|
          @type_resolver.resolve(
            signature_type: argument_type_parameter.type.sub(substitution),
            owner_full_name:
          )
        end

      ResolvedType.new(
        class_identifiers:
          resolved_argument_parameter_types
            .flat_map(&:class_identifiers)
            .uniq
            .sort,
        array_variant_class_identifiers: []
      )
    end

    def resolve_block_parameter_class_identifier(
      collected_method:,
      owner_full_name:,
      substitution:
    )

      collected_method.method_types.each do |method_type|
        method_block = method_type.block

        if method_block
          block_parameter = method_block.type.required_positionals.first

          unless block_parameter
            block_parameter = method_block.type.optional_positionals.first
          end

          if block_parameter
            resolved_block_parameter_type =
              @type_resolver.resolve(
                signature_type: block_parameter.type.sub(substitution),
                owner_full_name:
              )

            return resolved_block_parameter_type.class_identifiers.first || 0
          end
        end
      end
      0
    end
  end
end
