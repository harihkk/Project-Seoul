// Resolved only by tsconfig.local.json. The Chromium build supplies the real
// //resources/lit/v3_0/lit.rollup.js declarations.
export const nothing: unknown;
export function html(strings: TemplateStringsArray, ...values: unknown[]): unknown;
export function svg(strings: TemplateStringsArray, ...values: unknown[]): unknown;

export class CrLitElement extends HTMLElement {
  readonly updateComplete: Promise<unknown>;
  static get properties(): Record<string, unknown>;
  static get styles(): unknown;
  render(): unknown;
  connectedCallback(): void;
  disconnectedCallback(): void;
}
