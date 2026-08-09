# frozen_string_literal: true

require_relative "test_helper"

class TiDatabaseGeneratorTest < Minitest::Test
  include TiDatabaseGeneratorTestHelper

  def test_signature_paths_in_directory_are_sorted
    Dir.mktmpdir do |temporary_directory_path|
      File.write(File.join(temporary_directory_path, "b.rbs"), "")
      File.write(File.join(temporary_directory_path, "a.rbs"), "")

      assert_equal(
        [
          File.join(temporary_directory_path, "a.rbs"),
          File.join(temporary_directory_path, "b.rbs")
        ],
        TiDatabaseGenerator.signature_paths_in_directory(
          signature_directory_path: temporary_directory_path
        )
      )
    end
  end

  def test_signature_paths_in_directory_rejects_missing_directory
    Dir.mktmpdir do |temporary_directory_path|
      assert_raises(RuntimeError) do
        TiDatabaseGenerator.signature_paths_in_directory(
          signature_directory_path:
            File.join(temporary_directory_path, "missing")
        )
      end
    end
  end

  def test_signature_paths_in_directory_rejects_empty_directory
    Dir.mktmpdir do |temporary_directory_path|
      assert_raises(RuntimeError) do
        TiDatabaseGenerator.signature_paths_in_directory(
          signature_directory_path: temporary_directory_path
        )
      end
    end
  end

  def test_collector_method_kinds_initialize_alias_and_private
    collected_classes =
      build_collected_declarations_for(
        signature_file_names: ["method_kinds.rbs"]
      ).collected_classes

    collected_class = collected_classes.fetch("::Kinds")

    assert collected_class.static_methods.key?("both")
    assert collected_class.instance_methods.key?("both")

    assert collected_class.static_methods.key?("new")
    assert collected_class.instance_methods.key?("copy")

    refute collected_class.instance_methods.key?("hidden")
  end

  def test_signature_keeps_optional_arguments_keywords_blocks_and_overloads
    collected_classes =
      build_collected_declarations_for(
        signature_file_names: ["arguments.rbs", "overloads.rbs"]
      ).collected_classes

    rendered_signature =
      TiDatabaseGenerator::SignatureRenderer.new.render_method_signature(
        collected_method:
          collected_classes.fetch("::Arguments").instance_methods.fetch("call")
      )

    assert_includes rendered_signature, "?String optional"
    assert_includes rendered_signature, "key: Integer"
    assert_includes rendered_signature, "?timeout: Integer"
    assert_includes rendered_signature, "{ (String) -> void }"

    rendered_overload_signature =
      TiDatabaseGenerator::SignatureRenderer.new.render_method_signature(
        collected_method: collected_classes.fetch("::Overloads").instance_methods.fetch("convert")
      )

    assert_equal 2, rendered_overload_signature.lines.length
  end

  def test_return_type_resolves_optional_alias_array_self_literal_and_unsupported
    collected_declarations =
      build_collected_declarations_for(
        signature_file_names:
          ["basic.rbs", "aliases.rbs", "optional.rbs", "unsupported_types.rbs"]
      )

    collected_classes = collected_declarations.collected_classes

    class_identifiers_by_full_name =
      collected_classes.keys.sort.each_with_index.to_h do |full_class_name, index|
        [full_class_name, index + 1]
      end

    ["::Untyped", "::NilClass", "::Array"].each do |full_class_name|
      unless class_identifiers_by_full_name.key?(full_class_name)
        class_identifiers_by_full_name[full_class_name] =
          class_identifiers_by_full_name.length + 1
      end
    end

    type_resolver =
      TiDatabaseGenerator::TypeResolver.new(
        type_aliases: collected_declarations.type_aliases,
        class_identifiers_by_full_name:
      )

    optional_type = RBS::Parser.parse_type("String?")

    assert_equal(
      [
        class_identifiers_by_full_name["::String"],
        class_identifiers_by_full_name["::NilClass"]
      ].sort,
      type_resolver
        .resolve(signature_type: optional_type, owner_full_name: "::Basic")
        .class_identifiers
    )

    resolved_array_type =
      type_resolver.resolve(
        signature_type: RBS::Parser.parse_type("Array[String]"),
        owner_full_name: "::Basic"
      )

    assert_equal(
      [class_identifiers_by_full_name["::Array"]],
      resolved_array_type.class_identifiers
    )

    assert_equal(
      [class_identifiers_by_full_name["::String"]],
      resolved_array_type.array_variant_class_identifiers
    )

    resolved_hash_type =
      type_resolver.resolve(
        signature_type: RBS::Parser.parse_type("Hash[String, Integer]"),
        owner_full_name: "::Basic"
      )

    assert_equal(
      [class_identifiers_by_full_name["::Hash"]],
      resolved_hash_type.class_identifiers
    )

    assert_empty resolved_hash_type.array_variant_class_identifiers

    resolved_generic_alias_type =
      type_resolver.resolve(
        signature_type: RBS::Parser.parse_type("maybe[String]"),
        owner_full_name: "::Aliases"
      )

    assert_equal(
      [
        class_identifiers_by_full_name["::String"],
        class_identifiers_by_full_name["::NilClass"]
      ].sort,
      resolved_generic_alias_type.class_identifiers
    )

    resolved_nested_alias_type =
      type_resolver.resolve(
        signature_type: RBS::Parser.parse_type("nested_text"),
        owner_full_name: "::Aliases"
      )

    assert_equal(
      [class_identifiers_by_full_name["::String"]],
      resolved_nested_alias_type.class_identifiers
    )

    optional_variable_return_type =
      RBS::Parser
        .parse_method_type("[T] () -> T?")
        .type
        .return_type
        .sub(
          RBS::Substitution.build(
            [:T],
            [RBS::Parser.parse_type("String")]
          )
        )

    resolved_bound_optional_type =
      type_resolver.resolve(
        signature_type: optional_variable_return_type,
        owner_full_name: "::Optional"
      )

    assert_equal(
      [
        class_identifiers_by_full_name["::String"],
        class_identifiers_by_full_name["::NilClass"]
      ].sort,
      resolved_bound_optional_type.class_identifiers
    )

    resolved_unsupported_type =
      type_resolver.resolve(
        signature_type: RBS::Parser.parse_type("[String, Integer]"),
        owner_full_name: "::Basic"
      )

    assert_equal(
      [class_identifiers_by_full_name["::Untyped"]],
      resolved_unsupported_type.class_identifiers
    )
  end

  def test_inherited_class_type_variable_resolves_to_type_argument
    collected_declarations =
      build_collected_declarations_for(
        signature_file_names: ["basic.rbs", "type_variables.rbs"]
      )

    builtin_database =
      TiDatabaseGenerator::DatabaseBuilder.new.build(
        collected_declarations:
      )

    class_identifier =
      builtin_database.class_identifiers_by_full_name.fetch("::StringBox")

    builtin_class = builtin_database.builtin_classes.fetch(class_identifier)

    builtin_method =
      builtin_database.builtin_methods[
        builtin_class.instance_method_start_index,
        builtin_class.instance_method_count
      ].find do |builtin_method_entry|
        builtin_database
          .name_pool.binary_string
          .byteslice(builtin_method_entry.name_offset..)
          .split("\0", 2).first == "value"
      end

    assert_equal(
      builtin_database.class_identifiers_by_full_name.fetch("::String"),
      builtin_method.return_class_identifier
    )
  end

  def test_inherited_class_type_variable_is_bound_through_multiple_ancestors
    collected_declarations =
      build_collected_declarations_for(
        signature_file_names: ["basic.rbs", "multilevel_type_variables.rbs"]
      )

    builtin_database =
      TiDatabaseGenerator::DatabaseBuilder.new.build(
        collected_declarations:
      )

    class_identifier =
      builtin_database.class_identifiers_by_full_name.fetch("::StringMiddle")

    builtin_class =
      builtin_database.builtin_classes.fetch(class_identifier)

    builtin_method =
      builtin_database.builtin_methods[
        builtin_class.instance_method_start_index,
        builtin_class.instance_method_count
      ].find do |builtin_method_entry|
        builtin_database.name_pool.binary_string
          .byteslice(builtin_method_entry.name_offset..)
          .split("\0", 2)
          .first == "value"
      end

    assert_equal(
      builtin_database.class_identifiers_by_full_name.fetch("::String"),
      builtin_method.return_class_identifier
    )
  end

  def test_builtin_arguments_merge_overload_types_by_position
    collected_declarations =
      build_collected_declarations_for(
        signature_file_names: ["basic.rbs", "overloads.rbs"]
      )

    builtin_database =
      TiDatabaseGenerator::DatabaseBuilder.new.build(
        collected_declarations:
      )

    class_identifier =
      builtin_database.class_identifiers_by_full_name.fetch("::Overloads")

    builtin_class = builtin_database.builtin_classes.fetch(class_identifier)

    builtin_method =
      builtin_database.builtin_methods[
        builtin_class.instance_method_start_index,
        builtin_class.instance_method_count
      ].find do |builtin_method_entry|
        builtin_database.name_pool.binary_string
          .byteslice(builtin_method_entry.name_offset..)
          .split("\0", 2)
          .first == "convert"
      end

    assert_equal 1, builtin_method.argument_count

    builtin_argument =
      builtin_database.builtin_arguments.fetch(
        builtin_method.argument_start_index
      )

    union_entry =
      builtin_database.union_pool.union_entries.fetch(
        builtin_argument.union_index
      )

    assert_equal(
      [
        builtin_database.class_identifiers_by_full_name.fetch("::Integer"),
        builtin_database.class_identifiers_by_full_name.fetch("::String")
      ].sort,
      union_entry.reject(&:zero?).sort
    )

    assert_equal(
      [
        builtin_database.class_identifiers_by_full_name.fetch("::Integer"),
        builtin_database.class_identifiers_by_full_name.fetch("::String")
      ].sort,
      [
        builtin_method.return_class_identifier,
        *builtin_database.union_pool.union_entries
          .fetch(builtin_method.return_union_index)
          .reject(&:zero?)
      ].uniq.sort
    )
  end

  def test_builtin_arguments_preserve_rbs_argument_order_names_and_kinds
    collected_declarations =
      build_collected_declarations_for(
        signature_file_names: ["arguments.rbs", "basic.rbs"]
      )

    builtin_database =
      TiDatabaseGenerator::DatabaseBuilder.new.build(
        collected_declarations:
      )

    arguments_class_identifier =
      builtin_database.class_identifiers_by_full_name.fetch("::Arguments")

    arguments_builtin_class =
      builtin_database.builtin_classes.fetch(arguments_class_identifier)

    call_builtin_method =
      builtin_database.builtin_methods[
        arguments_builtin_class.instance_method_start_index,
        arguments_builtin_class.instance_method_count
      ].find do |builtin_method_candidate|
        builtin_database.name_pool.binary_string
          .byteslice(builtin_method_candidate.name_offset..)
          .split("\0", 2)
          .first == "call"
      end

    call_builtin_arguments =
      builtin_database.builtin_arguments[
        call_builtin_method.argument_start_index,
        call_builtin_method.argument_count
      ]

    assert_equal(
      TiDatabaseGenerator::BUILTIN_ARGUMENT_KINDS.values_at(
        :required,
        :optional,
        :rest,
        :required,
        :required_keyword,
        :optional_keyword,
        :rest_keyword
      ),
      call_builtin_arguments.map(&:kind)
    )

    builtin_argument_names =
      call_builtin_arguments.map do |builtin_argument|
        next if builtin_argument.name_offset.zero?

        builtin_database.name_pool.binary_string
          .byteslice(builtin_argument.name_offset..)
          .split("\0", 2)
          .first
      end

    assert_equal(
      [nil, nil, nil, nil, "key", "timeout", nil],
      builtin_argument_names
    )
  end

  def test_string_pool_limits
    string_pool = TiDatabaseGenerator::StringPool.new(pool_name: "test")

    assert_raises(RuntimeError) do
      string_pool.add_string_and_return_offset(string: "x" * 65_535)
    end
  end

end
