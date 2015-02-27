# Change Log
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased][unreleased]
### Fixed
- Fixed CDS phase correction on the reverse strand
- Corrected non-deterministic selection of primary isoform when multiple isoforms have the same (maximum) length
- In addition to correcting gene features, `AgnPseudogeneFixStream` also now corrects transcript and exon features as well.
### Changed
- Locations printed by Xtractore (of the form `seqid_start-end`) now include a strand character if strand is defined
- Began using [this specification](http://keepachangelog.com/) for maintaining a ChangeLog.
### Added
- Python script for merging iLoci
- Documentation for GFF3 expectations, both in general and tool-specific terms

