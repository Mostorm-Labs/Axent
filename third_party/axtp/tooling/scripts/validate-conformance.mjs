#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";

const root = path.resolve(process.argv[2] ?? process.cwd());
const requireFromGenerators = createRequire(path.join(root, "tooling", "generators", "package.json"));
const YAML = requireFromGenerators("yaml");
const Ajv2020Module = requireFromGenerators("ajv/dist/2020");
const Ajv2020 = Ajv2020Module.default ?? Ajv2020Module;

const conformanceDir = path.join(root, "conformance");
const manifestPath = path.join(conformanceDir, "manifest.yaml");
const caseSchemaPath = path.join(conformanceDir, "schemas", "conformance-case.schema.json");
const resultSchemaPath = path.join(conformanceDir, "schemas", "conformance-result.schema.json");
const generatedProtocolPath = path.join(root, "contract", "generated", "protocol.json");
const fixturesDir = path.join(conformanceDir, "fixtures", "device_profiles");

const errors = [];
const profileCounts = new Map();

function fail(message) {
  errors.push(message);
}

function readYaml(file) {
  return YAML.parse(fs.readFileSync(file, "utf8"));
}

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

function walkYaml(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walkYaml(full));
    else if (entry.isFile() && entry.name.endsWith(".yaml")) out.push(full);
  }
  return out.sort();
}

function caseIdFromPath(file) {
  const relative = path.relative(path.join(conformanceDir, "cases"), file);
  return relative.replace(/\.yaml$/, "").split(path.sep).join(".");
}

function formatAjvError(error) {
  const pathLabel = error.instancePath || "/";
  return `${pathLabel} ${error.message}`;
}

function expectObject(value, label) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    fail(`${label} must be an object`);
    return false;
  }
  return true;
}

function expectString(value, label) {
  if (typeof value !== "string" || value.length === 0) {
    fail(`${label} must be a non-empty string`);
    return false;
  }
  return true;
}

function statusNames(value) {
  if (typeof value === "string") return [value];
  if (value && typeof value === "object" && Array.isArray(value.oneOf)) return value.oneOf.map(String);
  return [];
}

function validateKnownStatus(label, value, errorNames) {
  for (const name of statusNames(value)) {
    if (!errorNames.has(name)) fail(`${label} references unknown status/error: ${name}`);
  }
}

function validateMethod(label, name, methodNames) {
  if (!name || name.startsWith("vendor.")) return;
  if (!methodNames.has(name)) fail(`${label} references unknown method: ${name}`);
}

function validateEvent(label, name, eventNames) {
  if (!name) return;
  if (!eventNames.has(name)) fail(`${label} references unknown event: ${name}`);
}

function validateCaseReferences(label, value, refs) {
  if (!refs.levelNames.has(value.level)) fail(`${label}.level references unknown manifest level: ${value.level}`);

  for (const profile of value.profiles ?? []) {
    profileCounts.set(profile, (profileCounts.get(profile) ?? 0) + 1);
    if (!refs.profileLabels.has(profile)) fail(`${label}.profiles references unknown profile/level: ${profile}`);
  }

  if (value.given?.device_profile && !refs.fixtureNames.has(value.given.device_profile)) {
    fail(`${label}.given.device_profile references missing fixture: ${value.given.device_profile}`);
  }
  validateMethod(`${label}.given.method`, value.given?.method, refs.methodNames);

  for (const [index, step] of (value.steps ?? []).entries()) {
    const stepLabel = `${label}.steps[${index}]`;
    validateMethod(`${stepLabel}.rpc.method`, step.rpc?.method, refs.methodNames);
    validateMethod(`${stepLabel}.jsonrpc.d.method`, step.jsonrpc?.d?.method, refs.methodNames);
    validateKnownStatus(`${stepLabel}.control.statusCode`, step.control?.statusCode, refs.errorNames);
    validateKnownStatus(`${stepLabel}.error.code`, step.error?.code, refs.errorNames);
    validateEvent(`${stepLabel}.event.name`, step.event?.name, refs.eventNames);

    if (step.registry_lookup?.capability && !refs.capabilityNames.has(step.registry_lookup.capability)) {
      fail(`${stepLabel}.registry_lookup.capability references unknown capability: ${step.registry_lookup.capability}`);
    }
    for (const method of step.registry_lookup?.methods ?? []) {
      validateMethod(`${stepLabel}.registry_lookup.methods`, method, refs.methodNames);
    }

    const expect = step.expect ?? {};
    validateMethod(`${stepLabel}.expect.jsonrpc.d.method`, expect.jsonrpc?.d?.method, refs.methodNames);
    validateKnownStatus(`${stepLabel}.expect.rpc.statusCode`, expect.rpc?.statusCode, refs.errorNames);
    validateKnownStatus(`${stepLabel}.expect.control.statusCode`, expect.control?.statusCode, refs.errorNames);
    validateKnownStatus(`${stepLabel}.expect.error.code`, expect.error?.code, refs.errorNames);
    validateEvent(`${stepLabel}.expect.event.name`, expect.event?.name, refs.eventNames);
  }
}

if (!fs.existsSync(manifestPath)) fail(`Missing ${path.relative(root, manifestPath)}`);
if (!fs.existsSync(caseSchemaPath)) fail(`Missing ${path.relative(root, caseSchemaPath)}`);
if (!fs.existsSync(resultSchemaPath)) fail(`Missing ${path.relative(root, resultSchemaPath)}`);
if (!fs.existsSync(generatedProtocolPath)) fail(`Missing ${path.relative(root, generatedProtocolPath)}`);

const caseSchema = readJson(caseSchemaPath);
const resultSchema = readJson(resultSchemaPath);
const ajv = new Ajv2020({ allErrors: true, strict: false });
const validateCaseSchema = ajv.compile(caseSchema);
ajv.compile(resultSchema);

const manifest = readYaml(manifestPath);
expectObject(manifest, "manifest");
expectObject(manifest.conformance, "manifest.conformance");
expectObject(manifest.levels, "manifest.levels");
expectString(manifest.conformance?.version, "manifest.conformance.version");
expectString(manifest.conformance?.spec_min, "manifest.conformance.spec_min");

const generated = readJson(generatedProtocolPath);
const refs = {
  methodNames: new Set((generated.methods ?? []).map((item) => item.name)),
  eventNames: new Set((generated.events ?? []).map((item) => item.name)),
  errorNames: new Set((generated.errors ?? []).map((item) => item.name)),
  capabilityNames: new Set((generated.capabilities ?? []).map((item) => item.name)),
  levelNames: new Set(Object.keys(manifest.levels ?? {})),
  profileLabels: new Set(Object.keys(manifest.levels ?? {})),
  fixtureNames: new Set()
};

for (const file of walkYaml(path.join(conformanceDir, "profiles"))) {
  const value = readYaml(file);
  const label = path.relative(root, file);
  expectObject(value, label);
  expectString(value?.id, `${label}.id`);
  expectString(value?.since, `${label}.since`);
  if (typeof value?.id === "string") refs.profileLabels.add(value.id);
}

if (fs.existsSync(fixturesDir)) {
  for (const entry of fs.readdirSync(fixturesDir, { withFileTypes: true })) {
    if (entry.isFile() && entry.name.endsWith(".yaml")) {
      refs.fixtureNames.add(entry.name.replace(/\.yaml$/, ""));
    }
  }
}

const caseFiles = walkYaml(path.join(conformanceDir, "cases"));
const casesById = new Map();
for (const file of caseFiles) {
  const label = path.relative(root, file);
  const value = readYaml(file);
  if (!validateCaseSchema(value)) {
    for (const error of validateCaseSchema.errors ?? []) fail(`${label}: ${formatAjvError(error)}`);
  }
  const expectedId = caseIdFromPath(file);
  if (value?.id !== expectedId) fail(`${label} id ${value?.id} does not match path id ${expectedId}`);
  if (typeof value?.id === "string") {
    if (casesById.has(value.id)) fail(`Duplicate case id: ${value.id}`);
    casesById.set(value.id, { file, value });
  }
  validateCaseReferences(label, value, refs);
}

for (const [level, value] of Object.entries(manifest.levels ?? {})) {
  if (!expectObject(value, `manifest.levels.${level}`)) continue;
  const requiredCases = value.required_cases;
  if (!Array.isArray(requiredCases)) {
    fail(`manifest.levels.${level}.required_cases must be an array`);
    continue;
  }
  for (const id of requiredCases) {
    if (!casesById.has(id)) fail(`manifest level ${level} references missing case: ${id}`);
  }
}

if (errors.length > 0) {
  console.error("[FAIL] conformance validation failed");
  for (const error of errors) console.error(`- ${error}`);
  process.exit(1);
}

console.log(`[OK] conformance manifest: ${manifest.conformance.version} (${manifest.conformance.spec_min})`);
console.log(`[OK] conformance cases: ${casesById.size}`);
for (const [profile, count] of [...profileCounts.entries()].sort(([a], [b]) => a.localeCompare(b))) {
  console.log(`[OK] profile coverage: ${profile}=${count}`);
}
