# Reference artifacts

Public reference material may include scripts, schemas, checksums and reviewed
summary reports. Original disc images, retail files, executables, saves,
registry exports, WER data and memory dumps must not be committed.

`reference/private/` is the default destination for machine-local M0 reports
and is ignored by Git. A report moves out of that directory only after it has
been checked for private data and reduced to evidence that is safe to publish.

Reviewed aggregate reports live in [`reports/`](reports/README.md). Their
manifest hashes bind them to the complete private evidence without publishing
the retail file lists themselves.
