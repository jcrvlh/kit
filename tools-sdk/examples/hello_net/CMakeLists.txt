cmake_minimum_required(VERSION 3.16)
project(hello_net C)

# Só o build Xtensa: `kit-cli build . --target xtensa` compila src/*.c direto
# (gera tool.so) e só confere que este arquivo existe. Este exemplo não tem
# modo nativo — a rede só responde no KIT (o stub fica sempre offline).
