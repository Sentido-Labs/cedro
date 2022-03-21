#pragma Cedro 1.0
// ngx_http_sqlite_module.c#L734

r @ ngx_http_sqlite_echo...
    ("\"", 1),
    ((char *)sqlite3_column_name(stmt, i), ngx_strlen(sqlite3_column_name(stmt, i))),
    ("\":\"", 3),
    ((char *)result, ngx_strlen(result)),
    ("\"", 1);
