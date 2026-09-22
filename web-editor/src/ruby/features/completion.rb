module Completion
  IDENTIFIER_CHARACTERS =
    'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_'

  class Session
    def initialize(editor, popup)
      @editor = editor
      @popup = popup
      @candidates = []
      @selected_index = 0
      @replace_start = 0
    end

    def attach
      @editor.addEventListener('refresh-completion') do
        refresh_completion
      end

      @editor.addEventListener('close-completion') do
        close_completion
      end
    end

    # Called from the synchronous keydown handler. Returns true when the popup used the key.
    def handle_keydown(event)
      return false if @candidates.empty?

      case event[:key].to_s
      when 'ArrowDown'
        move_selection(1)
      when 'ArrowUp'
        move_selection(-1)
      when 'Tab'
        move_selection(event[:shiftKey] ? -1 : 1)
      when 'Enter'
        apply_candidate
      when 'Escape'
        close_completion
      when 'ArrowLeft', 'ArrowRight', 'Home', 'End', 'PageUp', 'PageDown'
        close_completion
        return false
      else
        return false
      end

      event.preventDefault
      true
    rescue StandardError => e
      p 'completion keydown error'
      p e
      false
    end

    private

    def refresh_completion
      return close_completion unless JS.global[:tiReady]

      source = @editor[:value].to_s
      cursor_position = @editor[:selectionStart].to_i
      name_start = find_name_start(source, cursor_position)

      return close_completion unless name_start > 0 &&
                                     source[name_start - 1] == '.'

      arguments = JS.global.create_array
      arguments.push(source)
      arguments.push(cursor_position)
      suggestions = JS.global[:tiSuggest].apply(nil, arguments)

      @candidates = (0...suggestions[:length].to_i).map do |index|
        suggestion = suggestions[index]

        {
          contents: suggestion[:contents].to_s,
          detail: suggestion[:detail].to_s
        }
      end

      return close_completion if @candidates.empty?

      @selected_index = 0
      @replace_start = name_start

      render_popup(source, cursor_position)
    rescue StandardError => e
      p 'refresh completion error'
      p e
      close_completion
    end

    def find_name_start(source, cursor_position)
      name_start = cursor_position

      while name_start > 0 &&
            IDENTIFIER_CHARACTERS.include?(source[name_start - 1])
        name_start -= 1
      end

      name_start
    end

    def render_popup(source, cursor_position)
      @popup[:innerHTML] = make_candidates_html

      left, top =
        TextPosition.caret_bottom_left(@editor, source, cursor_position)

      @popup[:style][:left] = "#{left}px"
      @popup[:style][:top] = "#{top}px"
      @popup[:hidden] = false
    end

    def move_selection(step)
      @selected_index = (@selected_index + step) % @candidates.length
      @popup[:innerHTML] = make_candidates_html

      selected_item = @popup.querySelector('.completion-item.selected')
      return if selected_item.nil?

      scroll_options = JS.global.create_object
      scroll_options[:block] = 'nearest'
      selected_item.scrollIntoView(scroll_options)
    end

    def apply_candidate
      contents = @candidates[@selected_index][:contents]
      cursor_position = @editor[:selectionStart].to_i

      @editor.setSelectionRange(@replace_start, cursor_position)
      @editor.setRangeText(contents)

      new_cursor_position = @replace_start + contents.length
      @editor.setSelectionRange(new_cursor_position, new_cursor_position)

      close_completion

      @editor.dispatchEvent(JS.global[:Event].new('refresh-highlight'))
      @editor.dispatchEvent(JS.global[:Event].new('refresh-line-numbers'))
    end

    def close_completion
      @candidates = []
      @popup[:hidden] = true
      @popup[:innerHTML] = ''
    end

    def make_candidates_html
      (0...@candidates.length).map { |index|
        make_candidate_html(@candidates[index], index == @selected_index)
      }.join
    end

    def make_candidate_html(candidate, selected)
      cls = selected ? 'completion-item selected' : 'completion-item'

      "<div class=\"#{cls}\">" \
        "<span class=\"completion-contents\">#{escape_html(candidate[:contents])}</span>" \
        "<span class=\"completion-detail\">#{escape_html(candidate[:detail])}</span>" \
        '</div>'
    end

    def escape_html(text)
      text.gsub('&', '&amp;').gsub('<', '&lt;').gsub('>', '&gt;')
    end
  end
end
