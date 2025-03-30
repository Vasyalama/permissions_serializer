# Управление правами доступа
## about
FLTK gui app written in c++ that serializes/deserializes given file or folder along with its permissions. The permissions for different systems (windows and linux) are stored separately in the .kser file. On windows the current users permissions are serialized and deserialized (works with the 6 standard permissions and the 12 special permissions on windows). Deserializing on a system for which no permissions are saved in the .kser file results in creating the files with default permissions for the user. 

### serialize
  - pick an input folder or file to serialize
  - pick an output folder where to save the .kser file. Default: parent directory of input.
  - NOTE: if you want to save previously serialized permissions (from a different OS) for the input then provide the .kser file that was used to get the deserialized input. The app will overrite the new permissions and keep the permissions from other OS.
    
### deserialize
  - pick a .kser file to deserialize as input parameter
  - pick a folder to deserialze into. Default: parent directory of input.
  - will not work if output folder already contains an object with the same name as the resulting deserialized object.

## how data in .kser is stored.
data | file_obj_num | is_dir | filename_len | filename | win_perms| linux_perms| filesize | ... | raw_binary_file_data | ... | 
--- | --- | --- | --- |--- |--- |--- |--- |--- |--- |--- |
bytes | 4 | 1 | 4 | filename_len |4|4|8 |... | filesize | ... |    

NOTE: raw file data is stored without compression, so .kser files will take up about same amount of space as all the input files combined.

### prerequisits: [FLTK](https://www.fltk.org/) 
#### how to build fltk with cmake
- download fltk-1.4.2-source.tar.gz from [here](https://www.fltk.org/software.php)
- extract to any folder and navigate to FLTK root directory
- Generate the build system in the FLTK root directory, Build FLTK with the generated build system, Install FLTK
  
```
cmake -B build
cmake --build build
cmake --install build
```


### how to build project
```
git clone https://github.com/Vasyalama/permissions_serializer.git
cd permissions_serializer
mkdir build
cd build
cmake ..
cmake --build .
./CMakeProject1.exe
```


