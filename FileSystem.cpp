#include <raymath.h>
#include "FileSystem.h"

FileSystem *FileSystem::fileSystem = new FileSystem;
bool FileSystem::loaded = false;

int64_t FileSystem::File::indexCount = 0;
int64_t FileSystem::Folder::indexCount = 0;

FileSystem::File::File(string name, int64_t size, int64_t dataOffset, bool hidden, bool readOnly)
        : size(size), dataOffset(dataOffset) {
    this->id = indexCount++;
    this->name = std::move(name);
    this->hidden = hidden;
    this->readOnly = readOnly;
}

FileSystem::Folder::Folder(string name, int64_t contentOffset, bool hidden, bool readOnly)
        : contentOffset(contentOffset) {
    this->id = indexCount++;
    this->name = std::move(name);
    this->hidden = hidden;
    this->readOnly = readOnly;
}


FileSystem::File::File(int64_t id, string name, int64_t size, int64_t dataOffset, bool hidden, bool readOnly)
        : File(name, size, dataOffset, hidden, readOnly) {
    this->id = id;
}

FileSystem::Folder::Folder(int64_t id, string name, int64_t contentOffset, bool hidden, bool readOnly)
        : Folder(name, contentOffset, hidden, readOnly) {
    this->id = id;
}

FileSystem *FileSystem::getSystem() {
    if (!loaded) {
        if (fileSystem->loadData() || fileSystem->setup())
            loaded = true;
    }
    return fileSystem;
}

bool FileSystem::loadData() {
    char *ptr = new char[32];
    auto *ptr8 = reinterpret_cast<int64_t *>(ptr);

    int64_t signature = 8171306834495268337;

    ifstream loader;
    loader.open(systemMetaFile, ios::in | ios::binary);
    if (loader.is_open()) {
        loader.read(ptr, 8);
        if (*ptr8 != signature)
            return false;
        loader.read(ptr, 8);
        File::indexCount = *ptr8;
        loader.read(ptr, 8);
        Folder::indexCount = *ptr8;
        loader.close();
    } else
        return false;

    loader.open(filesMetaFile, ios::in | ios::binary);
    if (loader.is_open()) {
        loader.read(ptr, 8);
        if (*ptr8 != signature)
            return false;

        while (!loader.eof()) {
            loader.read(ptr, 8);
            int64_t id = *ptr8;
            if (loader.eof())
                break;
            if (id == -1) {
                loader.ignore(56);
                continue;
            }
            loader.read(ptr, 8);
            int64_t size = *ptr8;
            loader.ignore(8); // attributes
            loader.read(ptr, 32);
            string name{ptr};
            loader.read(ptr, 8);
            int64_t offset = *ptr8;
            fileIndexer[id] = File{id, name, size, offset};
        }
        loader.close();
    } else
        return false;

    loader.open(foldersMetaFile, ios::in | ios::binary);
    if (loader.is_open()) {
        loader.read(ptr, 8);
        if (*ptr8 != signature)
            return false;

        while (!loader.eof()) {
            loader.read(ptr, 8);
            int64_t id = *ptr8;
            if (loader.eof())
                break;
            if (id == -1) {
                loader.ignore(56);
                continue;
            }
            loader.ignore(8); // count files + count folder
            loader.ignore(8); // attributes
            loader.read(ptr, 32);
            string name{ptr};
            loader.read(ptr, 8);
            int64_t offset = *ptr8;
            folderIndexer[id] = {id, name, offset};
        }
        loader.close();
    } else
        return false;

    loader.open(fileDataFile, ios::in | ios::binary);
    if (loader.is_open()) {
        loader.read(ptr, 8);
        if (*ptr8 != signature)
            return false;
        loader.close();
    } else
        return false;

    loader.open(folderContentFile, ios::in | ios::binary);
    if (loader.is_open()) {
        loader.read(ptr, 8);
        if (*ptr8 != signature)
            return false;

        for (auto &pair: folderIndexer) {
            auto &folder = pair.second;
            loader.seekg(folder.contentOffset, ios::beg);
            loader.read(ptr, 8);
            while (*ptr8 != -1) {
                loader.seekg(*ptr8, ios::beg);
                loader.read(ptr, 8);
                if (*ptr != -1)
                    folder.files.push_back(*ptr);
                loader.read(ptr, 8);
            }
            loader.seekg(folder.contentOffset + 8, ios::beg);
            loader.read(ptr, 8);
            while (*ptr8 != -1) {
                loader.seekg(*ptr8, ios::beg);
                loader.read(ptr, 8);
                if (*ptr != -1) folder.folders.push_back(*ptr);
                loader.read(ptr, 8);
            }
        }
        loader.close();
    } else
        return false;

    currentFolder = getRoot();
    return true;
}

bool FileSystem::setup() {
    if (!initializeDataFiles())
        return false;
    createFolder("ROOT", nullptr);
    createFolder("RECYCLE BIN", getRoot());
    currentFolder = getRoot();
    return true;
}

bool FileSystem::initializeFile(const char *filename) {
    ofstream setter{filename, ios::out | ios::binary | ios::trunc};
    if (setter.is_open()) {
        setter.write(reinterpret_cast<const char *>(new int64_t{8171306834495268337L}), 8);
        setter.close();
        return true;
    }
    return false;
}

bool FileSystem::initializeDataFiles() {
    if (!initializeFile(systemMetaFile))
        return false;
    if (!initializeFile(filesMetaFile))
        return false;
    if (!initializeFile(foldersMetaFile))
        return false;
    if (!initializeFile(fileDataFile))
        return false;
    if (!initializeFile(folderContentFile))
        return false;
    return true;
}

void FileSystem::insertFile(const char *filePath, FileSystem::Folder *parent) {
    string str{filePath};
    auto pos = str.find_last_of('\\');
    auto index = pos != -1 ? pos : 0;
    string filename = str.substr(index);

    fstream stream{filePath, ios::in | ios::binary};
    if (stream.is_open()) {
        int64_t filesize = getFileSize(stream);
        auto offset = addFileData(stream, filesize);
        stream.close();

        auto *file = new File{filename, filesize, offset};
        registerFileToFolder(file, parent);
        registerFile(file);
    }
}

int64_t FileSystem::addFileData(fstream &stream, int64_t filesize) {
    const int64_t bufferSize = 1024;
    char *buffer = new char[1024];

    ofstream dataFile{fileDataFile, ios::out | ios::binary | ios::app};
    if (stream.is_open() && dataFile.is_open()) {
        dataFile.seekp(0, ios::end);
        int64_t offset = dataFile.tellp();
        int64_t pointer = 0;
        while (!stream.eof()) {
            if (pointer + bufferSize > filesize) {
                int64_t lastBufferSize = filesize - pointer;
                stream.read(buffer, lastBufferSize);
                dataFile.write(buffer, lastBufferSize);
                break;
            }

            stream.read(buffer, bufferSize);
            dataFile.write(buffer, bufferSize);

            pointer += bufferSize;
        }
        dataFile.close();
        return offset;
    }
    return -1;
}

void FileSystem::retrieveFile(const FileSystem::File *file, const char *saveAs) {
    if (file == nullptr)
        return;
    ofstream stream{saveAs != nullptr ? saveAs : file->name.c_str(), ios::out | ios::binary | ios::trunc};
    ifstream dataFile{fileDataFile, ios::in | ios::binary};
    if (stream.is_open() && dataFile.is_open()) {
        transferData(stream, dataFile, file->dataOffset, file->size);
        dataFile.close();
        stream.close();
    }
}

void
FileSystem::transferData(ofstream &destination, ifstream &source, int64_t offset, int64_t filesize) {

    const int64_t bufferSize = 1024;
    char *buffer = new char[1024];

    source.seekg(offset, ios::beg);
    int64_t pointer = 0;
    while (!source.eof()) {
        if (pointer + bufferSize > filesize) {
            int64_t lastBufferSize = filesize - pointer;
            source.read(buffer, lastBufferSize);
            destination.write(buffer, lastBufferSize);
            return;
        }
        source.read(buffer, bufferSize);
        destination.write(buffer, bufferSize);
        pointer += bufferSize;
    }
}

int64_t FileSystem::getFileSize(fstream &stream) {
    auto begin = stream.tellp();
    stream.seekp(0, ios::end);
    int64_t filesize = stream.tellp() - begin;
    stream.seekp(0, ios::beg);
    return filesize;
}

void FileSystem::createFolder(const char *name, Folder *parent) {
    fstream contentFile{folderContentFile, ios::out | ios::app | ios::binary};
    if (contentFile.is_open()) {
        contentFile.seekp(0, ios::end);
        int64_t offset = contentFile.tellp();
        contentFile.write(reinterpret_cast<const char *>(new int64_t{-1L}), 8); // Item ID
        contentFile.write(reinterpret_cast<const char *>(new int64_t{-1L}), 8); // next Item offset
        contentFile.close();

        auto *folder = new Folder{name, offset};
        if (folder->id > 0)
            registerFolderToFolder(folder, parent);
        registerFolder(folder);
    }

}

void FileSystem::deleteFile(const FileSystem::File *file, FileSystem::Folder *parent) {
    unregisterFileFromFolder(file, parent);
    registerFileToFolder(file, getRecycleBin());
}

void FileSystem::permanentDeleteFile(const FileSystem::File *file, FileSystem::Folder *parent) {
    unregisterFileFromFolder(file, parent);
    unregisterFile(file);
}

void FileSystem::deleteFolder(const FileSystem::Folder *folder, FileSystem::Folder *parent) {
    unregisterFolderFromFolder(folder, parent);
    registerFolderToFolder(folder, getRecycleBin());
}

void FileSystem::permanentDeleteFolder(const FileSystem::Folder *folder, FileSystem::Folder *parent) {
    unregisterFolderFromFolder(folder, parent);
    unregisterFolder(folder);
}

void FileSystem::unregisterFileFromFolder(const File *file, Folder *parent) {
    parent->files.remove(file->id);

    char *ptr = new char[8];
    auto *data = reinterpret_cast<int64_t *>(ptr);
    fstream content{folderContentFile, ios::in | ios::out | ios::binary};
    if (content.is_open()) {
        int64_t pos = getIdOffset(content, file->id, parent->contentOffset);
        if (pos == -1)
            return;
        content.seekp(pos, ios::beg);
        content.write(reinterpret_cast<const char *>(new int64_t{-1L}), 8);
        content.close();
    }
}

void FileSystem::unregisterFolderFromFolder(const FileSystem::Folder *folder, FileSystem::Folder *parent) {
    parent->folders.remove(folder->id);

    char *ptr = new char[8];
    auto *data = reinterpret_cast<int64_t *>(ptr);
    fstream content{folderContentFile, ios::in | ios::out | ios::binary};
    if (content.is_open()) {
        int64_t pos = getIdOffset(content, folder->id, parent->contentOffset + 8);
        if (pos == -1)
            return;
        content.seekp(pos, ios::beg);
        content.write(reinterpret_cast<const char *>(new int64_t{-1L}), 8);
        content.close();
    }
}

void FileSystem::registerFileToFolder(const File *file, Folder *parent) {
    parent->files.push_back(file->id);


    fstream contentFile{folderContentFile, ios::out | ios::in | ios::binary};
    if (contentFile.is_open()) {
        contentFile.seekp(0, ios::end);
        int64_t offset = contentFile.tellp();
        contentFile.write(reinterpret_cast<const char *>(&file->id), 8); // Item ID
        contentFile.write(reinterpret_cast<const char *>(new int64_t{-1L}), 8); // next Item offset

        writeNextOffset(contentFile, offset, parent->contentOffset);

        contentFile.close();
    }
}

void FileSystem::registerFolderToFolder(const Folder *folder, Folder *parent) {
    parent->folders.push_back(folder->id);


    fstream content{folderContentFile, ios::out | ios::in | ios::binary};
    if (content.is_open()) {
        content.seekp(0, ios::end);
        int64_t offset = content.tellp();
        content.write(reinterpret_cast<const char *>(&folder->id), 8); // Item ID
        content.write(reinterpret_cast<const char *>(new int64_t{-1L}), 8); // next Item offset

        writeNextOffset(content, offset, parent->contentOffset + 8);

        content.close();
    }
}

void FileSystem::writeNextOffset(fstream &content, int64_t &offset, int64_t initPos) {
    content.seekp(initPos, ios::beg);

    char *ptr = new char[8];
    auto *off = reinterpret_cast<int64_t *>(ptr);
    content.read(ptr, 8);
    while (*off != -1L) {
        content.seekp(*off + 8, ios::beg);
        content.read(ptr, 8);
    }
    content.seekp(-8, ios::cur);
    content.write(reinterpret_cast<const char *>(&offset), 8);
}

int64_t FileSystem::getIdOffset(fstream &content, int64_t targetId, int64_t initPos) {
    content.seekp(initPos, ios::beg);

    char *ptr = new char[8];
    auto *ptr8 = reinterpret_cast<int64_t *>(ptr);
    content.read(ptr, 8);
    while (*ptr8 != -1) {
        content.seekp(*ptr8, ios::beg);
        content.read(ptr, 8);
        if (*ptr8 == targetId) {
            int64_t resultPos = int64_t{content.tellp()} - 8;
            return resultPos;
        }
        content.read(ptr, 8);
    }
    return -1;
}

void FileSystem::registerFile(const FileSystem::File *file) {
    fileIndexer[file->id] = *file;

    ofstream metaFile{filesMetaFile, ios::out | ios::app | ios::binary};
    if (metaFile.is_open()) {
        metaFile.write(reinterpret_cast<const char *>(&file->id), 8);
        metaFile.write(reinterpret_cast<const char *>(&file->size), 8);
        metaFile.write(reinterpret_cast<const char *>(new int64_t{0L}), 8); // attributes
        metaFile.write(reinterpret_cast<const char *>(postString(file->name.c_str()).c_str()), 32);
        metaFile.write(reinterpret_cast<const char *>(&file->dataOffset), 8);
        metaFile.close();
    }
}

void FileSystem::unregisterFile(const FileSystem::File *file) {
    fileIndexer.erase(file->id);

    char *ptr = new char[8];
    auto *id = reinterpret_cast<int64_t *>(ptr);
    fstream metaFile{filesMetaFile, ios::in | ios::out | ios::binary};
    if (metaFile.is_open()) {
        metaFile.ignore(8);
        while (!metaFile.eof()) {
            metaFile.read(ptr, 8);
            if (*id == file->id) {
                metaFile.seekp(-8, ios::cur);
                metaFile.write(reinterpret_cast<const char *>(new int64_t{-1L}), 8);
                return;
            }
            metaFile.ignore(56);
        }
        metaFile.close();
    }
}

void FileSystem::registerFolder(const FileSystem::Folder *folder) {
    folderIndexer[folder->id] = *folder;

    ofstream metaFolder{foldersMetaFile, ios::out | ios::app | ios::binary};
    if (metaFolder.is_open()) {
        metaFolder.write(reinterpret_cast<const char *>(&folder->id), 8);
        metaFolder.write(reinterpret_cast<const char *>(new int{static_cast<int>(folder->folders.size())}), 4);
        metaFolder.write(reinterpret_cast<const char *>(new int{static_cast<int>(folder->files.size())}), 4);
        metaFolder.write(reinterpret_cast<const char *>(new int64_t{0L}), 8); // attributes
        metaFolder.write(reinterpret_cast<const char *>(postString(folder->name.c_str()).c_str()), 32);
        metaFolder.write(reinterpret_cast<const char *>(&folder->contentOffset), 8);
        metaFolder.close();
    }
}

void FileSystem::unregisterFolder(const FileSystem::Folder *folder) {
    folderIndexer.erase(folder->id);

    char *ptr = new char[8];
    auto *id = reinterpret_cast<int64_t *>(ptr);
    fstream metaFolder{foldersMetaFile, ios::in | ios::out | ios::binary};
    if (metaFolder.is_open()) {
        metaFolder.ignore(8);
        while (!metaFolder.eof()) {
            metaFolder.read(ptr, 8);
            if (*id == folder->id) {
                metaFolder.seekp(-8, ios::cur);
                metaFolder.write(reinterpret_cast<const char *>(new int64_t{-1L}), 8);
                return;
            }
            metaFolder.ignore(56);
        }
        metaFolder.close();
    }

    for (auto &folderId: folder->folders)
        unregisterFolder(&folderIndexer[folderId]);
    for (auto &fileId: folder->files)
        unregisterFile(&fileIndexer[fileId]);
}

list<string> FileSystem::queryFolder(const Folder *folder) {
    list<string> query;
    for (auto folderId: folder->folders)
        query.push_back(folderIndexer[folderId].name);
    for (auto fileId: folder->files)
        query.push_back(fileIndexer[fileId].name);
    return query;
}

list<string> FileSystem::queryCurrentFolder() {
    return queryFolder(currentFolder);
}

FileSystem::Item *FileSystem::getCurrentSelectedItem() {
    return currentSelectItem;
}

FileSystem::Folder *FileSystem::getCurrentFolder() {
    return currentFolder;
}

FileSystem::Folder *FileSystem::getRoot() {
    return &folderIndexer[0L];
}

FileSystem::Folder *FileSystem::getRecycleBin() {
    return &folderIndexer[1];
}

void FileSystem::optimizeSystemFiles() {
    updateSystemFiles();

    int64_t signature = 8171306834495268337;

    ofstream newFileData{"storage.fs.temp", ios::out | ios::binary};
    ifstream fileData{fileDataFile, ios::in | ios::binary};
    if (newFileData.is_open() && fileData.is_open()) {
        newFileData.write(reinterpret_cast<const char *>(&signature), 8);

        for (auto &pair: fileIndexer) {
            auto &file = pair.second;
            auto newOffset = newFileData.tellp();
            transferData(newFileData, fileData, file.dataOffset, file.size);
            file.dataOffset = newOffset;
        }
        fileData.close();
        newFileData.close();

        remove(fileDataFile);
        rename("storage.fs.temp", fileDataFile);
    }

    ofstream newFileMeta{"files.meta.temp", ios::out | ios::binary};
    if (newFileMeta.is_open()) {
        newFileMeta.write(reinterpret_cast<const char *>(&signature), 8);
        for (const auto &pair: fileIndexer) {
            auto file = pair.second;
            newFileMeta.write(reinterpret_cast<const char *>(&file.id), 8);
            newFileMeta.write(reinterpret_cast<const char *>(&file.size), 8);
            newFileMeta.write(reinterpret_cast<const char *>(new int64_t{0L}), 8); // attributes
            newFileMeta.write(reinterpret_cast<const char *>(postString(file.name.c_str()).c_str()), 32);
            newFileMeta.write(reinterpret_cast<const char *>(&file.dataOffset), 8);
        }
        newFileMeta.close();

        remove(filesMetaFile);
        rename("files.meta.temp", filesMetaFile);
    }

    ofstream newFolderContent{"content.meta.temp", ios::out | ios::binary};
    if (newFolderContent.is_open()) {
        newFolderContent.write(reinterpret_cast<const char *>(&signature), 8);
        for (auto &pair: folderIndexer) {
            auto &folder = pair.second;
            folder.contentOffset = newFolderContent.tellp();
            int64_t off = folder.contentOffset;

            int64_t filesOffset = (!folder.files.empty()) ? off + 16 + folder.folders.size() * 16 : -1;
            int64_t foldersOffset = (!folder.folders.empty()) ? off + 16 : -1;
            newFolderContent.write(reinterpret_cast<const char *>(&filesOffset), 8);
            newFolderContent.write(reinterpret_cast<const char *>(&foldersOffset), 8);
            int index = 1;
            if (!folder.folders.empty()) {
                for (auto iterator = folder.folders.begin(); iterator != folder.folders.end(); ++iterator, ++index) {
                    newFolderContent.write(reinterpret_cast<const char *>(new int64_t{*iterator}), 8);
                    newFolderContent.write(reinterpret_cast<const char *>(new int64_t{
                            iterator != folder.folders.end() ? foldersOffset + index * 16 : -1L}), 8);
                }
            }
            index = 1;
            if (!folder.files.empty()) {
                for (auto iterator = folder.files.begin(); iterator != folder.files.end(); ++iterator, ++index) {
                    newFolderContent.write(reinterpret_cast<const char *>(new int64_t{*iterator}), 8);
                    newFolderContent.write(reinterpret_cast<const char *>(new int64_t{
                            iterator != folder.files.end() ? filesOffset + index * 16 : -1L}), 8);
                }
            }
        }
        newFolderContent.close();

        remove(folderContentFile);
        rename("content.meta.temp", folderContentFile);
    }

    ofstream newFolderMeta{"folders.meta.temp", ios::out | ios::binary};
    if (newFolderMeta.is_open()) {
        newFolderMeta.write(reinterpret_cast<const char *>(&signature), 8);
        for (const auto &pair: folderIndexer) {
            auto folder = pair.second;
            newFolderMeta.write(reinterpret_cast<const char *>(&folder.id), 8);
            newFolderMeta.write(reinterpret_cast<const char *>(new int{static_cast<int>(folder.folders.size())}), 4);
            newFolderMeta.write(reinterpret_cast<const char *>(new int{static_cast<int>(folder.folders.size())}), 4);
            newFolderMeta.write(reinterpret_cast<const char *>(new int64_t{0L}), 8); // attributes
            newFolderMeta.write(reinterpret_cast<const char *>(postString(folder.name.c_str()).c_str()), 32);
            newFolderMeta.write(reinterpret_cast<const char *>(&folder.contentOffset), 8);
        }
        newFolderMeta.close();

        remove(foldersMetaFile);
        rename("folders.meta.temp", foldersMetaFile);
    }
}

void FileSystem::updateSystemFiles() {
    fstream sysMeta{systemMetaFile, ios::out | ios::in | ios::binary};
    if (sysMeta.is_open()) {
        sysMeta.ignore(8);
        sysMeta.write(reinterpret_cast<const char *>(&File::indexCount), 8);
        sysMeta.write(reinterpret_cast<const char *>(&Folder::indexCount), 8);
        sysMeta.close();
    }
}

void FileSystem::saveSystem() {
    updateSystemFiles();
}

string FileSystem::postString(const char *str) {
    string result{str};
    for (int i = result.length(); i < 32; ++i)
        result += '\0';
    return result;
}

void FileSystem::resetSystem() {
    remove(systemMetaFile);
    remove(filesMetaFile);
    remove(foldersMetaFile);
    remove(fileDataFile);
    remove(folderContentFile);
}

bool FileSystem::openFolder(FileSystem::Folder *folder) {
    currentFolder = folder;
    return true;
}

string FileSystem::report() {
    string builder{};
    builder.append("Current Folder: ").append(currentFolder->name);
    builder.append("\n\n");

    for (auto folderId: currentFolder->folders) {
        auto folder = folderIndexer[folderId];
        builder
                .append(folder.name)
                .append("\n");
    }
    for (auto fileId: currentFolder->files) {
        auto file = fileIndexer[fileId];
        builder
                .append(file.name)
                .append("\t")
                .append(to_string(file.size))
                .append(" Bytes")
                .append("\n");
    }
    return builder;
}

FileSystem::Folder *FileSystem::getFolderByName(const string &name) {
    for (auto id: currentFolder->folders) {
        auto &folder = folderIndexer[id];
        if (folder.name == name)
            return &folder;
    }
    return nullptr;
}

FileSystem::File *FileSystem::getFileByName(const string &name) {
    for (auto id: currentFolder->files) {
        auto &file = fileIndexer[id];
        if (file.name == name)
            return &file;
    }
    return nullptr;
}

bool FileSystem::File::isFolder() {
    return false;
}

bool FileSystem::Folder::isFolder() {
    return true;
}

void FileSystem::retrieveFileByName(string name) {
    retrieveFile(getFileByName(name));
}
