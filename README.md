# Управление правами доступа
## about
FLTK gui app written in c++ that serializes/deserializes given file or folder along with its permissions. 
- serialize
  - pick an input folder or file to serialize
  - pick an output folder where to save the .kser file. Default: parent directory of input.
  - NOTE. if you want to save previously serialized permissions (from different OS) for the input then provide the .kser file that was used to deserialize the input. The app will overrite the new permissions and keep the permissions from other OS.
- deserialize
    - pick a .kser file to deserialize as input parameter
    - pick a folder to deserialze into. Default: parent directory of input.
Works on linux and windows.

## how data in .kser is stored.
| amount_of_file_objects | (| isDir |filename_len_in_bytes|filename|file size|) * amount_of_file_objects | raw_binary_file_data * amount_of_file_objects |
|         4 bytes        |  |1 byte |      4 bytes        |  ...   | 8 bytes |       

NOTE: raw file data is stored without compression, so .kser files will take up about same amount of space as all the input files combined.

### prerequisits: [FLTK](https://www.fltk.org/) 
#### how to build fltk with cmake
- download fltk-1.4.2-source.tar.gz from [here](https://www.fltk.org/software.php)
- extract to any folder and navigate to FLTK root directory
- Generate the build system in the FLTK root directory
``` cmake -B build ```
- Build FLTK with the generated build system
``` cmake --build build ```
- Install FLTK
``` cmake --install build ```

### how to build project
```git clone
cd
mkdir build
cd build
cmake ..
cmake --build .
./CMakeProject1.exe
```


