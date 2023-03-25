#ifndef FILE_SYSTEM_FILESYSTEM_H
#define FILE_SYSTEM_FILESYSTEM_H

#include <iostream>
#include <map>
#include <unordered_map>
#include <list>
#include <fstream>
#include <cstring>

using namespace std;

const char systemMetaFile[] = "sys.meta";
const char filesMetaFile[] = "files.meta";
const char foldersMetaFile[] = "folders.meta";
const char fileDataFile[] = "storage.fs";
const char folderContentFile[] = "content.meta";

class FileSystem {
public:
    struct Item {
        int64_t id{};
        string name{};
        bool hidden = false;
        bool readOnly = false;

        virtual bool isFolder() = 0;
    };

    struct File : Item {
        int64_t size{};
        int64_t dataOffset{};

        static int64_t indexCount;

        File() = default;

        File(string name, int64_t size, int64_t dataOffset, bool hidden = false, bool readOnly = false);

        File(int64_t id, string name, int64_t size, int64_t dataOffset, bool hidden = false, bool readOnly = false);

        bool isFolder() override;
    };

    struct Folder : Item {
        int64_t contentOffset{};
        static int64_t indexCount;

        list <int64_t> files;
        list <int64_t> folders;

        Folder() = default;

        Folder(string name, int64_t contentOffset, bool hidden = false, bool readOnly = false);

        Folder(int64_t id, string name, int64_t contentOffset, bool hidden = false, bool readOnly = false);

        bool isFolder() override;
    };

private:

    map<int64_t, File> fileIndexer;
    map<int64_t, Folder> folderIndexer;

    Folder *currentFolder{};
    Item *currentSelectItem{};

    FileSystem() = default;

    static FileSystem *fileSystem;
    static bool loaded;

    bool loadData();

    bool setup();

    bool initializeDataFiles();

    int64_t addFileData(fstream &stream, int64_t filesize);

    void registerFile(const File *file);

    void unregisterFile(const File *file);

    void registerFolder(const Folder *folder);

    void unregisterFolder(const Folder *folder);

    void registerFolderToFolder(const Folder *folder, Folder *parent);

    void registerFileToFolder(const File *file, Folder *parent);

    void unregisterFileFromFolder(const File *file, Folder *parent);

    void unregisterFolderFromFolder(const Folder *folder, Folder *parent);

    int64_t getIdOffset(fstream &content, int64_t targetId, int64_t initPos);

    void writeNextOffset(fstream &content, int64_t &offset, int64_t initPos);

    string postString(const char *str);

    bool initializeFile(const char *filename);

    void updateSystemFiles();

    void transferData(ofstream &destination, ifstream &source, int64_t offset, int64_t filesize);

public:

    static FileSystem *getSystem();

    void insertFile(const char *filePath, Folder *parent);

    static int64_t getFileSize(fstream &stream);

    void createFolder(const char *name, Folder *parent);

    void retrieveFile(const FileSystem::File *file, const char *saveAs = nullptr);

    void deleteFile(const File *file, Folder *parent);

    void permanentDeleteFile(const File *file, Folder *parent);

    void deleteFolder(const Folder *folder, Folder *parent);

    void permanentDeleteFolder(const Folder *folder, Folder *parent);

    Folder *getCurrentFolder();

    Item *getCurrentSelectedItem();

    Folder *getRecycleBin();

    void saveSystem();

    list <string> queryFolder(const Folder *folder);

    list <string> queryCurrentFolder();

    FileSystem::Folder *getFolderByName(const string &name);

    File *getFileByName(const string &name);

    Folder *getRoot();

    void optimizeSystemFiles();

    static void resetSystem();

    bool openFolder(Folder *folder);

    string report();

    void retrieveFileByName(string name);
};

#endif //FILE_SYSTEM_FILESYSTEM_H
