//
// 使用下面命令编译并安装到 pam 模块目录
// gcc -fPIC -fstack-protector -c pam_sqlite.c
// gcc -shared -o pam_sqlite.so pam_sqlite.o -lpam -lsqlite3
// cp pam_sqlite.so /usr/lib/security/
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_appl.h>
#include <syslog.h>

#include <sqlite3.h>

#define DB_PATH "/tmp/users.db"

// 核心函数：处理身份验证
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    const char *user;
    const char *password;
    int retval;

    const char* db_path = DB_PATH;
    if (argc >= 1) {
        db_path = argv[0];
    }

    pam_syslog(pamh, LOG_INFO, "db path: %s", db_path);

    // 1. 获取认证的用户名
    retval = pam_get_user(pamh, &user, "Username: ");
    if (retval != PAM_SUCCESS || user == NULL) {
	pam_syslog(pamh, LOG_ERR, "pam_get_user failed!");
        return PAM_AUTH_ERR;
    }

    // 2. 获取认证的密码
    retval = pam_get_authtok(pamh, PAM_AUTHTOK, &password, NULL);
    if (retval != PAM_SUCCESS) {
	pam_syslog(pamh, LOG_ERR, "pam_get_authtok failed!");
        return PAM_AUTH_ERR;
    }

    // 3. 查询 SQLite 数据库
    sqlite3 *db;
    sqlite3_stmt *res;

    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        pam_syslog(pamh, LOG_ERR, "sqlite open failed: %s", sqlite3_errmsg(db));
        return PAM_SERVICE_ERR;
    }

    const char *sql = "SELECT password FROM users WHERE username = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK) {
        pam_syslog(pamh, LOG_ERR, "sqlite prepare failed: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return PAM_SERVICE_ERR;
    }

    sqlite3_bind_text(res, 1, user, -1, SQLITE_STATIC);

    int step = sqlite3_step(res);
    int authenticated = 0;

    if (step == SQLITE_ROW) {
        const char *db_password = (const char *)sqlite3_column_text(res, 0);
        // 注意：生产环境应使用密码哈希（如 bcrypt）进行比对
        if (strcmp(password, db_password) == 0) {
            pam_syslog(pamh, LOG_NOTICE, "auth ok: %s", user);
            authenticated = 1;
        } else {
	    pam_syslog(pamh, LOG_NOTICE, "bad password: %s", user);
	}
    }

    sqlite3_finalize(res);
    sqlite3_close(db);

    if (authenticated == 0) {
        pam_syslog(pamh, LOG_NOTICE, "authenticated failed: %s", user);
    }

    return authenticated ? PAM_SUCCESS : PAM_AUTH_ERR;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    return PAM_SUCCESS;
}
