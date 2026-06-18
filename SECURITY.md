# Security Policy

## Development status

This project modifies a Windows executable and is currently pre-release. Do not use development builds without preserving a clean game installation and understanding the restoration procedure.

## Supported versions

No version is currently considered production-supported. Security and data-loss reports for the latest `main` branch are still welcome during development.

## Reporting a security issue

Open a GitHub issue only when the report does not expose sensitive information. For potentially exploitable behavior, unsafe file replacement, arbitrary-file modification, or another issue that should not be public immediately, use GitHub's private vulnerability reporting feature when available.

Include:

- patcher commit or version;
- operating environment;
- exact command used;
- complete patcher output;
- reproduction steps;
- expected and actual behavior;
- SHA-256 hashes where relevant.

Never attach or redistribute:

- `FalloutNV.exe`;
- other Fallout: New Vegas game files;
- xNVSE binaries;
- Steamless binaries;
- private account, device, or filesystem information.

## Antivirus detections

Executable patchers may trigger heuristic antivirus detections because they read, modify, and replace executable files. A detection must not automatically be dismissed as a false positive.

Before a public release, each artifact should:

- be produced by the documented GitHub Actions workflow;
- have a published SHA-256 hash;
- be scanned with multiple engines;
- be reproducible from the tagged source where practical;
- be investigated if a detection appears or changes.

Only download release artifacts from this repository or an official Nexus page linked from this repository.

## Backup behavior

The patcher is intended to create `FalloutNV.exe.gn4gb-backup` before replacing `FalloutNV.exe`. Reports involving a missing, overwritten, or unusable backup should be treated as high priority.
