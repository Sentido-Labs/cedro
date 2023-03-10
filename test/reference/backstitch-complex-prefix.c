// ngx_http_sqlite_module.c#L802

(*chain->last)->buf->pos = u_str;
(*chain->last)->buf->last = u_str + ns.len;
(*chain->last)->buf->memory = 1;
