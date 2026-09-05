import { copyFile, mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const icons = ['circle-dot', 'pause', 'refresh-ccw', 'refresh-ccw-dot', 'mic', 'mic-off', 'save'];
const sourceDirectory = resolve('node_modules/lucide-static/icons');
const targetDirectory = resolve('data/icons/lucide');

await mkdir(targetDirectory, { recursive: true });

for (const icon of icons) {
  const source = await readFile(resolve(sourceDirectory, `${icon}.svg`), 'utf8');
  const whiteIcon = source.replace('stroke="currentColor"', 'stroke="#ffffff"');
  await writeFile(resolve(targetDirectory, `${icon}.svg`), whiteIcon);
}

await copyFile(
  resolve('node_modules/lucide-static/LICENSE'),
  resolve(targetDirectory, 'LICENSE'),
);
