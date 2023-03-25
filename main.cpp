#include "FileSystem.h"
#include <iostream>

using namespace std;

int main() {
//    FileSystem::resetSystem();
    auto system = FileSystem::getSystem();
//    system->insertFile("dummy.txt", system->getRoot());
//    system->insertFile("lab5.docx", system->getRoot());
//    system->saveSystem();
//    system->retrieveFileByName("lab5.docx");
//    system->createFolder("f1", system->getCurrentFolder());
//    system->createFolder("f2", system->getCurrentFolder());
//    cout << system->report() << endl;
//
//    system->openFolder(system->getFolderByName("f1"));
//    system->createFolder("f11", system->getCurrentFolder());
//    system->deleteFolder(system->getFolderByName("f1"), system->getCurrentFolder());
    cout << system->report() << endl;
    system->openFolder(system->getFolderByName("RECYCLE BIN"));
    cout << system->report() << endl;
    system->openFolder(system->getFolderByName("f1"));
    cout << system->report() << endl;
}
