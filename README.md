A script for dumping DICOM tags from a batch of locally stored DICOM studies. It iterates over a directory containing studies in individual directories and extracts DICOM tags from each series. Tags are provided as command line arguments.

## Usage
```
fnodcdump in-directory [options]
```
Tags of each series are dumped into CSV file per the following example:
```
PatientID; StudyInstanceUID; SeriesDescription; ... other tags
01; 1.2...; series desc1; ...
01; 1.2...; series desc2; ...
02; 1.3...; series desc1; ...
```
Tags with no value (empty string "") or no tags are dumped as "EMPTY" (without double quotes).

Script additionally filters out series whose tags ImageType and SeriesDescription contain these strings/substrings:
* ImageType - derived, secondary, localizer - secondary image data, processed data or topograms/scouts
* SeriesDescription - topog, scout, dose, report, protocol
The filter can be disable with the command line option ```-sf```.

## Requirements
* fmt v11.1 or newer
* dcmtk v3.6.9 or newer
