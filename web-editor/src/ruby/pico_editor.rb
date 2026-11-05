class PicoEditor
  attr_reader :theme, :font_size

  def initialize(root)
    wrapper = create_element('div', 'pico-editor')

    line_numbers = create_element('div', 'pico-editor-line-numbers')
    line_numbers_inner = create_element('div', 'pico-editor-line-numbers-inner')
    line_numbers.appendChild(line_numbers_inner)

    highlight_layer = create_element('div', 'pico-editor-highlight-layer')
    highlight_content = create_element('div', 'pico-editor-highlight-content')
    highlight_layer.appendChild(highlight_content)

    @editor = create_element('textarea', 'pico-editor-textarea')
    @editor[:wrap] = 'off'
    @editor[:spellcheck] = false

    wrapper.appendChild(line_numbers)
    wrapper.appendChild(highlight_layer)
    wrapper.appendChild(@editor)
    root.appendChild(wrapper)

    completion_popup = create_popup('pico-editor-completion-popup')
    hover_popup = create_popup('pico-editor-hover-popup')

    @appearance_elements = [wrapper, completion_popup, hover_popup]
    self.theme = 'dark'
    self.font_size = 15

    SyntaxHighlight::Session.new(@editor, highlight_layer, highlight_content).attach
    AutoIndent::Session.new(@editor).attach
    LineNumbers::Session.new(@editor, line_numbers_inner).attach

    completion = Completion::Session.new(@editor, completion_popup)
    completion.attach
    hover = Hover::Session.new(@editor, hover_popup)
    hover.attach

    EditorEventHandler.attach(@editor, completion, hover)

    @editor.dispatchEvent(JS.global[:Event].new('refresh-highlight'))
    @editor.dispatchEvent(JS.global[:Event].new('refresh-line-numbers'))
    @editor.dispatchEvent(JS.global[:Event].new('sync-line-number-scroll'))
  end

  def value
    @editor[:value].to_s
  end

  def value=(text)
    @editor[:value] = text
    @editor.dispatchEvent(JS.global[:Event].new('input'))
  end

  def theme=(name)
    unless name == 'dark' || name == 'light'
      raise ArgumentError, "theme must be 'dark' or 'light'"
    end

    @theme = name
    @appearance_elements.each do |element|
      element.setAttribute('data-pico-theme', name)
    end
  end

  def font_size=(size)
    unless size.is_a?(Integer) && size >= 8 && size <= 48
      raise ArgumentError, 'font_size must be an integer between 8 and 48'
    end

    @font_size = size
    @appearance_elements.each do |element|
      element[:style].setProperty('--pico-font-size', "#{size}px")
    end

    @editor.dispatchEvent(JS.global[:Event].new('render-highlight-viewport'))
    @editor.dispatchEvent(JS.global[:Event].new('sync-line-number-scroll'))
    @editor.dispatchEvent(JS.global[:Event].new('close-completion'))
    @editor.dispatchEvent(JS.global[:Event].new('hide-hover'))
  end

  private

  def create_element(tag_name, class_name)
    element = JS.document.createElement(tag_name)
    element[:className] = class_name
    element
  end

  def create_popup(class_name)
    popup = create_element('div', class_name)
    popup[:hidden] = true
    JS.document[:body].appendChild(popup)
    popup
  end
end
