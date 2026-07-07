// Seoul canonical protocol: runtime validator.
// Implements exactly the JSON Schema draft 2020-12 subset documented in
// protocol/README.md. Anything outside that subset in a schema is a hard
// error, so schema authors cannot silently rely on unimplemented keywords.
// Dependency-free; runs in Node tests and inside the bundled Design Lab.

const SUPPORTED_KEYWORDS = new Set([
  '$schema', '$id', '$defs', '$ref',
  'title', 'description',
  'type', 'enum', 'const',
  'properties', 'required', 'additionalProperties', 'propertyNames',
  'maxProperties', 'minProperties',
  'items', 'minItems', 'maxItems',
  'oneOf',
  'pattern', 'minLength', 'maxLength',
  'minimum', 'maximum',
]);

function typeOf(value) {
  if (value === null) return 'null';
  if (Array.isArray(value)) return 'array';
  return typeof value; // 'string' | 'number' | 'boolean' | 'object'
}

function matchesType(value, type) {
  switch (type) {
    case 'object':
      return typeOf(value) === 'object';
    case 'array':
      return Array.isArray(value);
    case 'string':
      return typeof value === 'string';
    case 'boolean':
      return typeof value === 'boolean';
    case 'number':
      return typeof value === 'number' && Number.isFinite(value);
    case 'integer':
      return typeof value === 'number' && Number.isInteger(value);
    case 'null':
      return value === null;
    default:
      throw new Error(`unsupported type keyword: ${type}`);
  }
}

function deepEqual(a, b) {
  if (a === b) return true;
  if (typeOf(a) !== typeOf(b)) return false;
  if (Array.isArray(a)) {
    return a.length === b.length && a.every((v, i) => deepEqual(v, b[i]));
  }
  if (typeOf(a) === 'object') {
    const ka = Object.keys(a);
    const kb = Object.keys(b);
    return ka.length === kb.length && ka.every((k) => deepEqual(a[k], b[k]));
  }
  return false;
}

function resolvePointer(doc, pointer, sourceName) {
  const parts = pointer.split('/').slice(1).map((p) => p.replaceAll('~1', '/').replaceAll('~0', '~'));
  let node = doc;
  for (const part of parts) {
    if (node === undefined || node === null || !(part in node)) {
      throw new Error(`unresolvable $ref pointer #${pointer} in ${sourceName}`);
    }
    node = node[part];
  }
  return node;
}

/**
 * Resolves a $ref against the current schema document and the registry.
 * Returns {schema, doc, docName} so nested refs resolve in the right file.
 */
function resolveRef(ref, currentDoc, currentName, registry) {
  const hash = ref.indexOf('#');
  const file = hash === -1 ? ref : ref.slice(0, hash);
  const pointer = hash === -1 ? '' : ref.slice(hash + 1);
  let doc = currentDoc;
  let docName = currentName;
  if (file !== '') {
    doc = registry[file];
    docName = file;
    if (!doc) {
      throw new Error(`$ref to unregistered schema file: ${ref}`);
    }
  }
  const schema = pointer === '' ? doc : resolvePointer(doc, pointer, docName);
  return { schema, doc, docName };
}

function checkSupportedKeywords(schema, docName) {
  for (const key of Object.keys(schema)) {
    if (!SUPPORTED_KEYWORDS.has(key)) {
      throw new Error(`schema ${docName} uses unsupported keyword "${key}"`);
    }
  }
}

function validateNode(value, schema, ctx, path, errors) {
  if (errors.length >= ctx.maxErrors) return;
  checkSupportedKeywords(schema, ctx.docName);

  if (schema.$ref !== undefined) {
    const resolved = resolveRef(schema.$ref, ctx.doc, ctx.docName, ctx.registry);
    const subCtx = { ...ctx, doc: resolved.doc, docName: resolved.docName };
    validateNode(value, resolved.schema, subCtx, path, errors);
    // Sibling keywords next to $ref (only description in this corpus) are
    // annotations; validation keywords must not accompany $ref.
    return;
  }

  if (schema.const !== undefined && !deepEqual(value, schema.const)) {
    errors.push({ path, message: `must equal ${JSON.stringify(schema.const)}` });
    return;
  }

  if (schema.enum !== undefined && !schema.enum.some((e) => deepEqual(value, e))) {
    errors.push({ path, message: `must be one of ${schema.enum.slice(0, 8).join(', ')}${schema.enum.length > 8 ? ', …' : ''}` });
    return;
  }

  if (schema.oneOf !== undefined) {
    let matched = 0;
    let closest = null;
    for (const branch of schema.oneOf) {
      const branchErrors = [];
      validateNode(value, branch, ctx, path, branchErrors);
      if (branchErrors.length === 0) {
        matched += 1;
      } else if (closest === null || branchErrors.length < closest.length) {
        closest = branchErrors;
      }
    }
    if (matched !== 1) {
      if (matched === 0 && closest) {
        errors.push({ path, message: `matched no oneOf branch (closest failure: ${closest[0].path}: ${closest[0].message})` });
      } else if (matched > 1) {
        errors.push({ path, message: `matched ${matched} oneOf branches; exactly one required` });
      }
      return;
    }
  }

  if (schema.type !== undefined && !matchesType(value, schema.type)) {
    errors.push({ path, message: `expected ${schema.type}, got ${typeOf(value)}` });
    return;
  }

  if (typeof value === 'string') {
    if (schema.minLength !== undefined && value.length < schema.minLength) {
      errors.push({ path, message: `string shorter than minLength ${schema.minLength}` });
    }
    if (schema.maxLength !== undefined && value.length > schema.maxLength) {
      errors.push({ path, message: `string longer than maxLength ${schema.maxLength}` });
    }
    if (schema.pattern !== undefined && !ctx.patternCache.get(schema.pattern).test(value)) {
      errors.push({ path, message: `does not match pattern ${schema.pattern}` });
    }
  }

  if (typeof value === 'number') {
    if (schema.minimum !== undefined && value < schema.minimum) {
      errors.push({ path, message: `below minimum ${schema.minimum}` });
    }
    if (schema.maximum !== undefined && value > schema.maximum) {
      errors.push({ path, message: `above maximum ${schema.maximum}` });
    }
  }

  if (Array.isArray(value)) {
    if (schema.minItems !== undefined && value.length < schema.minItems) {
      errors.push({ path, message: `fewer than minItems ${schema.minItems}` });
    }
    if (schema.maxItems !== undefined && value.length > schema.maxItems) {
      errors.push({ path, message: `more than maxItems ${schema.maxItems}` });
    }
    if (schema.items !== undefined) {
      value.forEach((item, i) => validateNode(item, schema.items, ctx, `${path}[${i}]`, errors));
    }
  }

  if (typeOf(value) === 'object' && !Array.isArray(value)) {
    const keys = Object.keys(value);
    if (schema.minProperties !== undefined && keys.length < schema.minProperties) {
      errors.push({ path, message: `fewer than minProperties ${schema.minProperties}` });
    }
    if (schema.maxProperties !== undefined && keys.length > schema.maxProperties) {
      errors.push({ path, message: `more than maxProperties ${schema.maxProperties}` });
    }
    if (schema.required !== undefined) {
      for (const req of schema.required) {
        if (!(req in value)) {
          errors.push({ path, message: `missing required property "${req}"` });
        }
      }
    }
    if (schema.propertyNames !== undefined && schema.propertyNames.pattern !== undefined) {
      const re = ctx.patternCache.get(schema.propertyNames.pattern);
      for (const key of keys) {
        if (!re.test(key)) {
          errors.push({ path: `${path}.${key}`, message: `property name does not match ${schema.propertyNames.pattern}` });
        }
      }
    }
    const declared = schema.properties ?? {};
    for (const key of keys) {
      const child = `${path}.${key}`;
      if (key in declared) {
        validateNode(value[key], declared[key], ctx, child, errors);
      } else if (schema.additionalProperties === false) {
        errors.push({ path: child, message: 'unexpected property' });
      } else if (schema.additionalProperties !== undefined && schema.additionalProperties !== true) {
        validateNode(value[key], schema.additionalProperties, ctx, child, errors);
      }
    }
  }
}

class PatternCache {
  constructor() {
    this.map = new Map();
  }
  get(pattern) {
    let re = this.map.get(pattern);
    if (!re) {
      re = new RegExp(pattern, 'u');
      this.map.set(pattern, re);
    }
    return re;
  }
}

/**
 * Validates `value` against `schema`. `registry` maps schema file names
 * (e.g. "adaptive-surface.schema.json") to parsed schema documents, for
 * cross-file $refs. Returns { valid, errors: [{path, message}] }.
 */
export function validate(value, schema, registry = {}) {
  const errors = [];
  const ctx = {
    doc: schema,
    docName: schema.$id ?? '(root)',
    registry,
    patternCache: new PatternCache(),
    maxErrors: 64,
  };
  validateNode(value, schema, ctx, '$', errors);
  return { valid: errors.length === 0, errors };
}

/** Formats validator errors into one readable line each. */
export function formatErrors(errors) {
  return errors.map((e) => `${e.path}: ${e.message}`).join('\n');
}
