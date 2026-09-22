module EditorEventHandler
  def self.attach(editor, completion, hover)
    on editor, 'input' do
      editor.dispatchEvent(JS.global[:Event].new('auto-outdent'))
      editor.dispatchEvent(JS.global[:Event].new('refresh-highlight'))
      editor.dispatchEvent(JS.global[:Event].new('refresh-line-numbers'))
      editor.dispatchEvent(JS.global[:Event].new('refresh-completion'))
      editor.dispatchEvent(JS.global[:Event].new('hide-hover'))
    end

    on editor, 'scroll' do
      editor.dispatchEvent(JS.global[:Event].new('render-highlight-viewport'))
      editor.dispatchEvent(JS.global[:Event].new('sync-line-number-scroll'))
      editor.dispatchEvent(JS.global[:Event].new('close-completion'))
      editor.dispatchEvent(JS.global[:Event].new('hide-hover'))
    end

    on editor, 'selectionchange' do
      editor.dispatchEvent(JS.global[:Event].new('refresh-active-line-number'))
    end

    on editor, 'keydown', sync: true do |ev|
      next if completion.handle_keydown(ev)
      next unless ev[:key].to_s == 'Enter'

      ev.preventDefault
      editor.dispatchEvent(JS.global[:Event].new('auto-indent'))
      editor.dispatchEvent(JS.global[:Event].new('refresh-highlight'))
      editor.dispatchEvent(JS.global[:Event].new('refresh-line-numbers'))
    end

    on editor, 'mousedown' do
      editor.dispatchEvent(JS.global[:Event].new('close-completion'))
    end

    on editor, 'blur' do
      editor.dispatchEvent(JS.global[:Event].new('close-completion'))
    end

    on editor, 'mousemove' do |ev|
      hover.schedule(ev[:clientX].to_f, ev[:clientY].to_f)
    end

    on editor, 'mouseleave' do
      editor.dispatchEvent(JS.global[:Event].new('hide-hover'))
    end
  end

  def self.on(target, event_name, sync: false, &block)
    target.addEventListener(event_name, sync:, &block)
  end
end
