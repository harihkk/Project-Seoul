// Project Seoul Canvas wire types.
//
// These mirror the validated SAUI JSON that crosses the Mojo boundary. They
// intentionally describe data rather than DOM so rendering remains a closed,
// auditable transformation.

export interface SurfaceDoc {
  id: string;
  kind: string;
  title?: string;
  components: ComponentNode[];
  data?: Record<string, DataEntry>;
  actions?: SurfaceAction[];
}

export interface ComponentNode {
  id: string;
  type: string;
  props?: Record<string, unknown>;
  bindings?: Record<string, string>;
  accessible_name?: string;
  state?: string;
  state_message?: string;
  actions?: string[];
  children?: ComponentNode[];
}

export interface DataColumn {
  key: string;
  label: string;
}

export interface SeriesPoint {
  t_ms?: number;
  x?: number;
  y: number;
}

export interface DataEntry {
  kind: 'scalar'|'record'|'series'|'table';
  value?: unknown;
  fields?: Record<string, unknown>;
  columns?: DataColumn[];
  rows?: unknown[][];
  points?: SeriesPoint[];
}

export interface SurfaceAction {
  id: string;
  label: string;
  kind: string;
  target: string;
}

export interface TaskSnapshotDoc {
  id: string;
  goal: string;
  state: string;
  failure?: string;
  has_semantic_result?: boolean;
  pending_approval_step?: string;
  pending_approval_prompt?: string;
  pending_user_input?: boolean;
  receipts?: Array<{
    step_id: string;
    status: string;
    observed_summary?: string;
    verification?: {
      verified: boolean;
      method?: string;
      detail?: string;
    };
  }>;
}

export interface PageContextDoc {
  status: 'ready'|'unavailable';
  tab_id: string;
  title: string;
  origin: string;
  customizable: boolean;
}

export interface LibraryBoardElementDoc {
  id: string;
  kind: 'text'|'link'|'image_reference'|'capture_reference'|'surface_reference';
  title: string;
  text: string;
  reference: string;
  origin: string;
  x: number;
  y: number;
  width: number;
  height: number;
  z_index: number;
}

export interface LibraryBoardDoc {
  id: string;
  name: string;
  archived: boolean;
  created_at?: unknown;
  modified_at?: unknown;
  elements?: LibraryBoardElementDoc[];
}

export interface LibraryArtifactDoc {
  id: string;
  kind: string;
  title: string;
  origin: string;
  mime_type: string;
  pinned: boolean;
}

export interface LiveCollectionDoc {
  id: string;
  name: string;
  refresh_capability: string;
  enabled: boolean;
  refresh_state: string;
  last_error?: string;
  items?: Array<{
    stable_key: string;
    title: string;
    subtitle?: string;
    url?: string;
    status?: string;
    actionable?: boolean;
  }>;
}

export interface LibrarySnapshotDoc {
  schema_version?: number;
  status?: string;
  detail?: string;
  boards?: LibraryBoardDoc[];
  artifacts?: LibraryArtifactDoc[];
  live_collections?: LiveCollectionDoc[];
}

export interface StudioProviderRouteDoc {
  configured: boolean;
  healthy?: boolean;
  enabled?: boolean;
  available?: boolean;
  model_configured: boolean;
  discovered_model_count?: number;
  model?: string;
  voice_configured?: boolean;
}

export interface StudioSceneDoc {
  id: string;
  name: string;
  workspace_id: string;
  theme_id: string;
  site_layer_count: number;
  prefer_compact: boolean;
}

export interface StudioSiteLayerDoc {
  id: string;
  name: string;
  origin_pattern: string;
  scene_scope: string;
  enabled: boolean;
  adjustment_count: number;
}

export interface StudioThemeDoc {
  id: string;
  name: string;
  scheme: 'light'|'dark'|'system';
  background: string;
  surface: string;
  accent: string;
  corner_radius_px: number;
  reduced_motion: boolean;
  reduced_transparency: boolean;
}

export interface StudioSnapshotDoc {
  schema_version?: number;
  status?: string;
  detail?: string;
  providers?: {
    local?: StudioProviderRouteDoc;
    cloud?: StudioProviderRouteDoc;
  };
  scenes?: StudioSceneDoc[];
  themes?: StudioThemeDoc[];
  site_layers?: StudioSiteLayerDoc[];
}

export interface SiteLayerAdjustmentDoc {
  kind: string;
  selectors?: string[];
  color_value?: string;
  font_family?: string;
  numeric_value?: number;
  density?: 'compact'|'comfortable'|'spacious';
}

export interface SiteLayerDoc {
  schema_version: number;
  id: string;
  name: string;
  origin_pattern: string;
  scene_scope: string;
  enabled: boolean;
  adjustments: SiteLayerAdjustmentDoc[];
  matches_active_page?: boolean;
}

export interface SiteLayerSnapshotDoc {
  status?: string;
  detail?: string;
  schema_version?: number;
  active_page?: {
    tab_id: string;
    title: string;
    origin: string;
    customizable: boolean;
  };
  matching_enabled_count?: number;
  layers?: SiteLayerDoc[];
}

export function safeHexColor(value: unknown): string {
  return typeof value === 'string' &&
          /^#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?$/.test(value) ?
      value : 'transparent';
}

export function propString(
    props: Record<string, unknown>|undefined, key: string): string {
  const value = props?.[key];
  return typeof value === 'string' ? value : '';
}

export function safeHttpUrl(value: unknown): string|undefined {
  if (typeof value !== 'string') {
    return undefined;
  }
  try {
    const url = new URL(value);
    return url.protocol === 'http:' || url.protocol === 'https:' ?
        url.href : undefined;
  } catch {
    return undefined;
  }
}
