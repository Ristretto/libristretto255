echo "Running ci/test.bat"

@echo on

md build
md build\lib
md build\obj
md build\obj\bin

for %%c in (src\bool.c src\bzero.c src\arch\ref64\f_impl.c src\f_arithmetic.c src\f_generic.c src\ristretto.c src/ristretto_gen_tables.c src\scalar.c) do (
    cl.exe /W4 /I include /I src /I src\arch\ref64 /Fo.\build\obj\ %%c
)

link /out:build\obj\bin\ristretto_gen_tables.exe build\obj\*.obj
bin\obj\bin\ristretto_gen_tables > src\ristretto_tables.c
cl /W4 /I.\include /I.\src src\ristretto_tables.c /out:bin\obj\ristretto_tables.obj
lib /out:build\lib\libristretto255.lib build\obj\bool.obj build\obj\bzero.obj build\obj\f_impl.obj build\obj\f_arithmetic.o build\f_generic.obj build\ristretto.o build\scalar.obj

cd tests
cargo test --all --lib
