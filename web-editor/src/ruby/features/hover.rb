module Hover
  # Type inference takes about 30 ms on a 16 KB source, so wait until the mouse stops.
  HOVER_DELAY_MS = 300
  POPUP_OFFSET_Y = 16

  class Session
    def initialize(editor, popup)
      @editor = editor
      @popup = popup
      @mouse_x = 0.0
      @mouse_y = 0.0
      @mouse_inside = false
      @last_move_time = 0.0
      @timer_pending = false
    end

    def attach
      @editor.addEventListener('hide-hover') do
        @mouse_inside = false
        hide_popup
      end
    end

    def schedule(client_x, client_y)
      @mouse_x = client_x
      @mouse_y = client_y
      @mouse_inside = true
      @last_move_time = current_time

      hide_popup
      start_timer(HOVER_DELAY_MS) unless @timer_pending
    end

    private

    def start_timer(delay_ms)
      @timer_pending = true

      JS.global.setTimeout(delay_ms) do
        @timer_pending = false
        elapsed_ms = current_time - @last_move_time

        if elapsed_ms < HOVER_DELAY_MS
          start_timer((HOVER_DELAY_MS - elapsed_ms).ceil)
        else
          show_popup
        end
      end
    end

    def show_popup
      return unless @mouse_inside && JS.global[:tiReady]

      source = @editor[:value].to_s
      offset = TextPosition.offset_at(@editor, source, @mouse_x, @mouse_y)
      return if offset.nil?

      arguments = JS.global.create_array
      arguments.push(source)
      arguments.push(offset)
      hover = JS.global[:tiHover].apply(nil, arguments)
      return if hover.nil?

      @popup[:textContent] = make_hover_text(hover)
      @popup[:style][:left] = "#{@mouse_x}px"
      @popup[:style][:top] = "#{@mouse_y + POPUP_OFFSET_Y}px"
      @popup[:hidden] = false
    rescue StandardError => e
      p 'show hover error'
      p e
    end

    def make_hover_text(hover)
      if hover[:isMethod]
        method_signature = hover[:methodSignature].to_s
        method_document = hover[:methodDocument].to_s

        return method_signature if method_document.empty?

        "#{method_signature}\n#{method_document}"
      else
        "#{hover[:variableName]}: #{hover[:typeName]}"
      end
    end

    def hide_popup
      @popup[:hidden] = true
    end

    def current_time
      JS.global[:Date].now.to_f
    end
  end
end
