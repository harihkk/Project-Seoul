// Project Seoul Canvas prototype - entry point.
import { mountCanvas } from './canvas.js';

const root = document.getElementById('root');
if (root) {
  mountCanvas(root);
}
