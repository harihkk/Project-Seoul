// Type declarations for protocol/ts/validate.mjs.
export interface ValidationError {
  path: string;
  message: string;
}
export interface ValidationResult {
  valid: boolean;
  errors: ValidationError[];
}
export declare function validate(
  value: unknown,
  schema: object,
  registry?: Record<string, object>,
): ValidationResult;
export declare function formatErrors(errors: ValidationError[]): string;
