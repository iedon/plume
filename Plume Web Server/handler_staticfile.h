#pragma once

void Http_StaticFileHandler(HP_HttpServer pServer, HP_CONNID dwConnID);
void directory_listing(HP_HttpServer pServer, HP_CONNID dwConnID);
LPCSTR dirlist_GetFileTypeImgClass(const CStringPlus & path);
void PostFileTransfering(HP_HttpServer pServer, HP_CONNID dwConnID);
