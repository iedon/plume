#include <iostream>
#include <string>
#include "Windows.h"
#include "constants.h"

const char   *  APP_NAME						= "Plume";							// Ӧ������
const char   *  APP_VER							= "0.5.4";							// Ӧ�ð汾
const char   *  APP_FULL_NAME					= "Plume Web Server";				// Ӧ��ȫ��
const char	 *	APP_DESCRIPTION					= "iEdon Plume Web Server";			// Ӧ�÷�������
const char	 *  APP_COPYRIGHT					= "Copyright (C) 2012-2018 iEdon. All rights reserved.";
char         *  APP_DIRECTORY					= nullptr;							// Ӧ��Ŀ¼
std::string     APP_TOKEN						= "iEdon-";							// Plume/x.x.x

HANDLE			VAR_IMPERSONATE_TOKEN			= INVALID_HANDLE_VALUE;				// Ϊ��������ӽ����ṩ��ȫTOKEN

DWORD				VAR_MEM_ALLOCATION				= 0;					// ϵͳ�ڴ������С
std::string			VAR_CONFIG_FILE;										// �����ļ�
DWORD				VAR_MAX_CONNECTIONS				= 0;					// ���������
unsigned long long	VAR_LIMIT_BODY					= 0;					// ���� POST ��С
DWORD				VAR_PLUGIN_BUFFER_SIZE			= 0;					// ���ϵͳ��̬ҳ�����������С
bool				VAR_ENABLE_ACCESS_LOG			= false;				// �Ƿ��¼������־
bool				VAR_ENABLE_GZIP					= false;				// �Ƿ�����GZIP
bool				VAR_ENABLE_DIR_LISTING			= false;				// �Ƿ�����Ŀ¼�б�
std::wstring		VAR_LOG_DIRECTORY_PREFIX;								// ��־Ŀ¼ǰ׺
std::string			VAR_ADMIN_EMAIL;										// ����Ա����
std::string			VAR_UPLOAD_TEMP_DIR_PREFIX;								// �ϴ���ʱ���Ŀ¼
std::string			VAR_GZIP_CACHE_TEMP_DIR_PREFIX;							// GZIP�����ļ�Ŀ¼

std::vector<MIME_TYPE>			VAR_MIME_TYPES;							// ȫ��MIME����
std::vector<SERVER>				VAR_SERVERS;							// ȫ�ַ���˱�
std::vector<VIRTUAL_HOST>		VAR_VIRTUAL_HOSTS;						// ȫ������������
std::vector<HANDLER_MAP>		VAR_HANDLER_MAPS;						// ȫ�ִ���ӳ���
std::vector<SSL_CERT>			VAR_SSL_CERTS;							// ȫ��SSL֤���
std::vector<SNI_MAP>			VAR_SNI_MAPS;							// ȫ��SNIӳ���
