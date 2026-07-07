// Seoul Canvas Design Lab - the fixture capability catalog data.
// Statically imports the SHARED protocol fixture corpus (protocol/fixtures/),
// the same files the native C++ conformance tests read, and assembles the
// registered fixture capabilities from the catalog manifest. Everything here
// is synthetic demo data: running a fixture capability returns its canned
// semantic result document; no browser, provider, or network is observed.

import catalogManifest from '../../../protocol/fixtures/catalog.json';

import capPipelineLatency from '../../../protocol/fixtures/capability/pipeline-latency.json';
import capSurveyReadings from '../../../protocol/fixtures/capability/survey-readings.json';
import capNodeProfile from '../../../protocol/fixtures/capability/node-profile.json';
import capWorkspaceTree from '../../../protocol/fixtures/capability/workspace-tree.json';
import capReferenceCitations from '../../../protocol/fixtures/capability/reference-citations.json';
import capReliabilityMetric from '../../../protocol/fixtures/capability/reliability-metric.json';
import capMaterialsTable from '../../../protocol/fixtures/capability/materials-table.json';
import capMaintenanceWindows from '../../../protocol/fixtures/capability/maintenance-windows.json';
import capModuleGraph from '../../../protocol/fixtures/capability/module-graph.json';
import capStationCoordinates from '../../../protocol/fixtures/capability/station-coordinates.json';
import capCalibrationProcedure from '../../../protocol/fixtures/capability/calibration-procedure.json';
import capIntakeForm from '../../../protocol/fixtures/capability/intake-form.json';
import capMaintenanceActions from '../../../protocol/fixtures/capability/maintenance-actions.json';
import capSamplerDiff from '../../../protocol/fixtures/capability/sampler-diff.json';
import capModuleStructure from '../../../protocol/fixtures/capability/module-structure.json';
import capStatusDigest from '../../../protocol/fixtures/capability/status-digest.json';
import capArchivePartial from '../../../protocol/fixtures/capability/archive-inventory-partial.json';
import capQueueDepthStream from '../../../protocol/fixtures/capability/queue-depth-stream.json';
import capReadingDisagreement from '../../../protocol/fixtures/capability/reading-disagreement.json';
import capArchiveError from '../../../protocol/fixtures/capability/archive-inventory-error.json';

import semTimeSeries from '../../../protocol/fixtures/semantic/time-series.json';
import semCollection from '../../../protocol/fixtures/semantic/collection.json';
import semRecord from '../../../protocol/fixtures/semantic/record.json';
import semHierarchy from '../../../protocol/fixtures/semantic/hierarchy.json';
import semCitations from '../../../protocol/fixtures/semantic/citations.json';
import semScalar from '../../../protocol/fixtures/semantic/scalar.json';
import semTable from '../../../protocol/fixtures/semantic/table.json';
import semIntervals from '../../../protocol/fixtures/semantic/intervals.json';
import semGraph from '../../../protocol/fixtures/semantic/graph.json';
import semGeospatial from '../../../protocol/fixtures/semantic/geospatial.json';
import semDocument from '../../../protocol/fixtures/semantic/document.json';
import semForm from '../../../protocol/fixtures/semantic/form.json';
import semActionSet from '../../../protocol/fixtures/semantic/action-set.json';
import semDiff from '../../../protocol/fixtures/semantic/diff.json';
import semCode from '../../../protocol/fixtures/semantic/code.json';
import semComposite from '../../../protocol/fixtures/semantic/composite.json';
import semPartial from '../../../protocol/fixtures/semantic/partial-result.json';
import semStreaming from '../../../protocol/fixtures/semantic/streaming-update.json';
import semStreamingAppend from '../../../protocol/fixtures/semantic/streaming-update.append.json';
import semConflict from '../../../protocol/fixtures/semantic/source-conflict.json';
import semError from '../../../protocol/fixtures/semantic/error-result.json';

import type { CapabilityDescriptor, SemanticResult } from './protocol.js';

export interface FixtureCapability {
  descriptor: CapabilityDescriptor;
  /** The canned semantic result this capability returns. Synthetic. */
  result: SemanticResult;
  exampleGoal: string;
  /** Extra list-shape rows a streaming fixture appends per batch. */
  streamAppendRows?: Record<string, unknown>[];
}

const resultByPath: Record<string, unknown> = {
  'semantic/time-series.json': semTimeSeries,
  'semantic/collection.json': semCollection,
  'semantic/record.json': semRecord,
  'semantic/hierarchy.json': semHierarchy,
  'semantic/citations.json': semCitations,
  'semantic/scalar.json': semScalar,
  'semantic/table.json': semTable,
  'semantic/intervals.json': semIntervals,
  'semantic/graph.json': semGraph,
  'semantic/geospatial.json': semGeospatial,
  'semantic/document.json': semDocument,
  'semantic/form.json': semForm,
  'semantic/action-set.json': semActionSet,
  'semantic/diff.json': semDiff,
  'semantic/code.json': semCode,
  'semantic/composite.json': semComposite,
  'semantic/partial-result.json': semPartial,
  'semantic/streaming-update.json': semStreaming,
  'semantic/source-conflict.json': semConflict,
  'semantic/error-result.json': semError,
};

const descriptorByPath: Record<string, unknown> = {
  'capability/pipeline-latency.json': capPipelineLatency,
  'capability/survey-readings.json': capSurveyReadings,
  'capability/node-profile.json': capNodeProfile,
  'capability/workspace-tree.json': capWorkspaceTree,
  'capability/reference-citations.json': capReferenceCitations,
  'capability/reliability-metric.json': capReliabilityMetric,
  'capability/materials-table.json': capMaterialsTable,
  'capability/maintenance-windows.json': capMaintenanceWindows,
  'capability/module-graph.json': capModuleGraph,
  'capability/station-coordinates.json': capStationCoordinates,
  'capability/calibration-procedure.json': capCalibrationProcedure,
  'capability/intake-form.json': capIntakeForm,
  'capability/maintenance-actions.json': capMaintenanceActions,
  'capability/sampler-diff.json': capSamplerDiff,
  'capability/module-structure.json': capModuleStructure,
  'capability/status-digest.json': capStatusDigest,
  'capability/archive-inventory-partial.json': capArchivePartial,
  'capability/queue-depth-stream.json': capQueueDepthStream,
  'capability/reading-disagreement.json': capReadingDisagreement,
  'capability/archive-inventory-error.json': capArchiveError,
};

/** The fixture capabilities in catalog order, assembled from the manifest. */
export function loadFixtureCatalog(): FixtureCapability[] {
  return catalogManifest.entries.map((entry) => {
    const descriptor = descriptorByPath[entry.capability];
    const result = resultByPath[entry.result];
    if (!descriptor || !result) {
      throw new Error(`catalog entry references unbundled fixture: ${entry.capability} / ${entry.result}`);
    }
    const capability: FixtureCapability = {
      descriptor: descriptor as CapabilityDescriptor,
      result: result as SemanticResult,
      exampleGoal: entry.example_goal,
    };
    if (entry.result === 'semantic/streaming-update.json') {
      capability.streamAppendRows = (semStreamingAppend as { rows: Record<string, unknown>[] }).rows;
    }
    return capability;
  });
}

/** The plain-language synthetic-data notice shown wherever fixtures render. */
export const SYNTHETIC_DATA_NOTICE = catalogManifest.note;
