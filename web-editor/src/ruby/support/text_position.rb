# Convert between editor character offsets and viewport pixel positions.
module TextPosition
  # Matches tab-size in editor.css.
  TAB_SIZE = 2

  class << self
    def caret_bottom_left(editor, source, offset)
      metrics = make_metrics(editor)
      lines_before_caret = source[0, offset].to_s.split("\n", -1)

      line_index = [lines_before_caret.length - 1, 0].max
      line_text = lines_before_caret.last || ''

      [
        metrics[:left] + measure_line_width(metrics, line_text),
        metrics[:top] + (line_index + 1) * metrics[:line_height]
      ]
    end

    # Returns nil when the point is not over a character.
    def offset_at(editor, source, client_x, client_y)
      metrics = make_metrics(editor)
      lines = source.split("\n", -1)

      line_index = ((client_y - metrics[:top]) / metrics[:line_height]).floor
      return nil if line_index < 0 || line_index >= lines.length

      column = column_at(metrics, lines[line_index], client_x - metrics[:left])
      return nil if column.nil?

      line_start = 0
      line_index.times { |index| line_start += lines[index].length + 1 }

      line_start + column
    end

    private

    def make_metrics(editor)
      rect = editor.getBoundingClientRect
      style = JS.global.getComputedStyle(editor)

      context = canvas_context
      context[:font] = "#{style[:fontSize]} #{style[:fontFamily]}"

      {
        left: rect[:left].to_f + style[:paddingLeft].to_s.to_f -
          editor[:scrollLeft].to_f,
        top: rect[:top].to_f + style[:paddingTop].to_s.to_f -
          editor[:scrollTop].to_f,
        line_height: style[:lineHeight].to_s.to_f,
        space_width: measure_text_width(' ')
      }
    end

    # Returns the index of the first character whose right edge is past x.
    def column_at(metrics, line_text, x)
      return nil if x < 0

      low = 0
      high = line_text.length

      while low < high
        middle = (low + high) / 2

        if measure_line_width(metrics, line_text[0, middle + 1]) > x
          high = middle
        else
          low = middle + 1
        end
      end

      return nil if low >= line_text.length

      low
    end

    def measure_line_width(metrics, line_text)
      tab_width = metrics[:space_width] * TAB_SIZE
      width = 0.0

      line_text.split("\t", -1).each_with_index do |segment, index|
        width = ((width / tab_width).floor + 1) * tab_width if index > 0
        width += measure_text_width(segment) unless segment.empty?
      end

      width
    end

    def measure_text_width(text)
      canvas_context.measureText(text)[:width].to_f
    end

    def canvas_context
      @canvas_context ||=
        JS.document.createElement('canvas').getContext('2d')
    end
  end
end
