cmake_minimum_required(VERSION 3.16)
project(hello_wifi C)

# Só o build Xtensa: `kit-cli build . --target xtensa` compila src/*.c direto
# (gera tool.so) e só confere que este arquivo existe. O stub do desktop
# devolve uma vizinhança de mentira, mas o scan de verdade só no KIT.
