// ngx_http_sqlite_module.c#L734

ngx_http_sqlite_echo(r, "\"", 1);
ngx_http_sqlite_echo(r, (char *)sqlite3_column_name(stmt, i), ngx_strlen(sqlite3_column_name(stmt, i)));
ngx_http_sqlite_echo(r, "\":\"", 3);
ngx_http_sqlite_echo(r, (char *)result, ngx_strlen(result));
ngx_http_sqlite_echo(r, "\"", 1);
