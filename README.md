# fnodcdump
A command line tool for writing a multiple DICOM tags from a bulk of DICOM files. 

## Usage
`fnodcdump <input-directory> [options] `

#### Common Options:
`--tag (-t)`: additional DICOM tags to write. Contains tags `PatientID` and `StudyInstanceUID` (or `SeriesInstanceUID`) by default.  
`--series-level (-s)`: write tags at series level, default is study level.

#### Output:
Tags are written as `.csv` file in this format:
```
PatientID;Study/SeriesInstanceUID;TagName3;...
01;1.2...;Value1-3;...
01;1.2...;Value2-3;...
02;1.3...;Value2-3;...
```
Each row is a DICOM study or series. First and second columns always are `PatientID` and `StudyInstanceUID` (default) or `SeriesInstanceUID` if option `--series-level` is given. 


## Requirements
* fmt v11.1 or newer
* dcmtk v3.6.9 or newer
