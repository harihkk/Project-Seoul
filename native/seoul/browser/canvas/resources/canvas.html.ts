// Project Seoul Canvas Lit template. Kept separate so Chromium's checked-in
// html.ts tooling can audit the trusted template boundary.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SeoulCanvasAppElement} from './canvas.js';

export function getHtml(this: SeoulCanvasAppElement) {
  const activeTasks = this.activeTasks_();
  const voiceActive = this.voiceState_ === 'connecting' ||
      this.voiceState_ === 'listening' || this.voiceState_ === 'microphone_requesting';
  return html`<!--_html_template_start_-->
    <main id="canvas-root" aria-live="polite">
      <header class="canvas-header">
        <div><span class="eyebrow">SEOUL CANVAS</span>
          <h1>${this.selectedView_ === 'canvas' ?
              this.surface_?.title || 'Ask, act, understand.' :
              this.selectedView_ === 'studio' ? 'Shape how Seoul works.' :
              'Your thinking space'}</h1></div>
        <span class="route" aria-live="polite">${this.routeLabel_}</span>
      </header>

      <nav class="view-switcher" aria-label="Canvas views">
        ${(['canvas', 'library', 'boards', 'studio'] as const).map(view => html`
          <button type="button" aria-current="${this.selectedView_ === view ? 'page' : 'false'}"
              @click="${() => this.selectView_(view)}">${view[0]!.toUpperCase() + view.slice(1)}</button>`)}
      </nav>

      ${this.selectedView_ === 'canvas' && activeTasks.length ? html`<section class="task-ribbon" aria-label="Live tasks">
        ${activeTasks.map(task => html`<article class="task-row">
          <span class="task-state task-${task.state}">${task.state.replace(/_/g, ' ')}</span>
          <span class="task-goal">${task.goal}</span>
          <span class="task-controls">
            ${task.state === 'executing' ? html`<button @click="${() => this.taskControl_(task, 'pause')}">Pause</button>` : nothing}
            ${task.state === 'paused' ? html`<button @click="${() => this.taskControl_(task, 'resume')}">Resume</button>` : nothing}
            ${task.state === 'awaiting_approval' && !task.pending_user_input ? html`
              <button class="primary" @click="${() => this.taskControl_(task, 'approve')}">Approve</button>
              <button @click="${() => this.taskControl_(task, 'reject')}">Reject</button>` : nothing}
            ${task.state !== 'failed' ? html`<button @click="${() => this.taskControl_(task, 'cancel')}">Cancel</button>` : nothing}
          </span>
          ${task.pending_approval_prompt ? html`<p class="task-prompt">${task.pending_approval_prompt}</p>` : nothing}
          ${task.pending_user_input ? html`<form class="task-input"
              @submit="${(event: Event) => { event.preventDefault(); this.provideTaskInput_(task); }}">
            <input aria-label="Input for ${task.goal}" placeholder="Type the missing detail"
                .value="${this.taskInputs_[task.id] ?? ''}"
                @input="${(event: Event) => this.onTaskInput_(task, event)}">
            <button class="primary" type="submit"
                ?disabled="${!(this.taskInputs_[task.id] ?? '').trim()}">Continue</button>
          </form>` : nothing}
        </article>`)}
      </section>` : nothing}

      <section class="canvas-content" aria-label="Result surface">
        ${this.selectedView_ === 'library' ? this.renderLibrary_() :
          this.selectedView_ === 'boards' ? this.renderBoards_() :
          this.selectedView_ === 'studio' ? this.renderStudio_() :
          this.surface_ ? html`<div class="saui-surface">
            ${this.surface_.components.map(component => this.renderComponent_(component))}
          </div>` : html`<div class="idle">
            <div class="idle-orb" aria-hidden="true"></div>
            <h2>What should we do?</h2>
            <p>Ask a question, compare information, or tell Seoul to work in this browser.</p>
          </div>`}
      </section>
    </main>

    <footer class="composer">
      <button class="voice-button" type="button" aria-label="Toggle voice input"
          aria-pressed="${voiceActive}" data-state="${this.voiceState_}"
          data-configured="${this.voiceConfigured_}" @click="${this.toggleVoice_}">
        <span class="voice-dot" aria-hidden="true"></span><span>Voice</span>
      </button>
      <input type="text" aria-label="Message Seoul" placeholder="Ask or act…"
          .value="${this.inputValue_}" @input="${this.onInput_}"
          @keydown="${this.onInputKeydown_}">
      <button class="send-button" type="button" aria-label="Send message"
          ?disabled="${!this.inputValue_.trim()}" @click="${this.submitTurn_}">↑</button>
    </footer>
  <!--_html_template_end_-->`;
}
