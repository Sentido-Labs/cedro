// http://docs.libuv.org/en/v1.x/guide/threads.html#core-thread-operations
// `liebre` y `tortuga` son funciones.
int main() {
    int lon_pista = 10;
    uv_thread_t id_liebre;
    uv_thread_t id_tortuga;
    uv_thread_create(&id_liebre, liebre, &lon_pista);
    uv_thread_create(&id_tortuga, tortuga, &lon_pista);

    uv_thread_join(&id_liebre);
    uv_thread_join(&id_tortuga);
    return 0;
}
