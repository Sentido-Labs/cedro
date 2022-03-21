#pragma Cedro 1.0
// http://docs.libuv.org/en/v1.x/guide/threads.html#core-thread-operations
// `liebre` y `tortuga` son funciones.
int main() {
    int lon_pista = 10;
    @uv_thread_...
        t id_liebre,
        t id_tortuga,
        create(&id_liebre, liebre, &lon_pista),
        create(&id_tortuga, tortuga, &lon_pista),

        join(&id_liebre),
        join(&id_tortuga);
    return 0;
}
