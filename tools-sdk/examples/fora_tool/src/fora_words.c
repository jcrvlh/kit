/**
 * @file fora_words.c
 * @brief Banco de palavras para a Tool FORA.
 *
 * 20 categorias com 50+ palavras cada, todas em CAIXA ALTA (UTF-8).
 * Quando uma escape hex precede uma letra ASCII, usamos string
 * concatenation ("\\xNN" "LETRA") para evitar ambiguidade.
 */

#include "fora_words.h"

#define COUNTOF(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

/* -----------------------------------------------------------------------
 * Categoria 1: COMIDAS
 * ----------------------------------------------------------------------- */
static const char *const WORDS_COMIDAS[] = {
    "PIZZA", "LASANHA", "HAMB" "\xC3\x9A" "RGUER", "SUSHI", "TACO",
    "CHURRASCO", "FEIJOADA", "MACARR" "\xC3\x83" "O", "SOPA", "SALADA",
    "SANDUICHE", "PASTEL", "COXINHA", "P" "\xC3\x83" "O DE QUEIJO", "TAPIOCA",
    "CREPE", "RISOTO", "STROGONOFF", "BATATA FRITA", "PIPOCA",
    "BOLO", "PUDIM", "BRIGADEIRO", "SORVETE", "MOUSSE",
    "CHOCOLATE", "BISCOITO", "TORTA", "EMPADA", "A" "\xC3\x87" "A" "\xC3\x8D",
    "SASHIMI", "YAKISOBA", "ESFIRRA", "BURRITO", "NACHOS",
    "WAFFLE", "PANQUECA", "OMELETE", "MISTO QUENTE", "HOT DOG",
    "ACARAJ" "\xC3\x89", "ARROZ DOCE", "RABANADA", "PAMONHA", "CURAL",
    "FEIJ" "\xC3\x83" "O TROPEIRO", "QUICHE", "FONDUE", "CEVICHE", "TEMPURA",
    "CROISSANT", "BRUSCHETTA", "CARPACCIO", "CANOLI", "BAKLAVA",
    "ESPETINHO", "COSTELA", "PICANHA", "FRANGO FRITO", "ESTROGONOFE",
};

/* -----------------------------------------------------------------------
 * Categoria 2: BEBIDAS
 * ----------------------------------------------------------------------- */
static const char *const WORDS_BEBIDAS[] = {
    "CAF" "\xC3\x89", "CERVEJA", "CAIPIRINHA", "REFRIGERANTE", "SUCO",
    "CH" "\xC3\x81", "LEITE", "\xC3\x81" "GUA DE COCO", "VINHO", "CHAMPANHE",
    "WHISKY", "VODKA", "RUM", "GIN", "TEQUILA",
    "MOJITO", "LIMONADA", "MILKSHAKE", "SMOOTHIE", "ENERGETICO",
    "CHOCOLATE QUENTE", "CAPPUCCINO", "EXPRESSO", "LATTE", "MATE",
    "GUARAN" "\xC3\x81", "TODDY", "LICOR", "SANGRIA", "SPRITZ",
    "DAIQUIRI", "MARGARITA", "COSMOPOLITAN", "LONG ISLAND", "CUBA LIBRE",
    "CERVEJA ARTESANAL", "CHOPP", "CIDRA", "SAQUE", "GRAPPA",
    "SUCO VERDE", "KOMBUCHA", "HORCHATA", "CALDO DE CANA", "CHIMARR" "\xC3\x83" "O",
    "BATIDA", "PONCHE", "HIDROMEL", "ABSINTO", "APEROL",
    "MARTINI", "NEGRONI", "MANHATTAN", "GIN T" "\xC3\x94" "NICA", "PINA COLADA",
};

/* -----------------------------------------------------------------------
 * Categoria 3: ANIMAIS
 * ----------------------------------------------------------------------- */
static const char *const WORDS_ANIMAIS[] = {
    "CACHORRO", "GATO", "ELEFANTE", "BALEIA", "LE" "\xC3\x83" "O",
    "TIGRE", "URSO", "\xC3\x81" "GUIA", "TUBAR" "\xC3\x83" "O", "GOLFINHO",
    "CAVALO", "MACACO", "GIRAFA", "PINGUIM", "COBRA",
    "PAPAGAIO", "TARTARUGA", "POLVO", "FORMIGA", "BORBOLETA",
    "LOBO", "RAPOSA", "CORUJA", "JACAR" "\xC3\x89", "HIPOP" "\xC3\x93" "TAMO",
    "RINOCERONTE", "ZEBRA", "CAMELO", "CANGURU", "PANDA",
    "ARARA", "FLAMINGO", "TUCANO", "BEM-TE-VI", "BEIJA-FLOR",
    "HAMSTER", "COELHO", "OVELHA", "PORCO", "VACA",
    "GALINHA", "PATO", "PERU", "ABELHA", "MOSCA",
    "ARANHA", "ESCORPI" "\xC3\x83" "O", "CARACOL", "ESTRELA-DO-MAR", "CAVALO-MARINHO",
    "PREGUICA", "CAPIVARA", "TATU", "MORCEGO", "CORVO",
    "PEIXE-PALHACO", "LULA", "CARANGUEJO", "LAGOSTA", "SIRI",
};

/* -----------------------------------------------------------------------
 * Categoria 4: PAÍSES
 * ----------------------------------------------------------------------- */
static const char *const WORDS_PAISES[] = {
    "BRASIL", "JAP" "\xC3\x83" "O", "IT" "\xC3\x81" "LIA", "CANAD" "\xC3\x81", "FRAN" "\xC3\x87" "A",
    "ALEMANHA", "ESPANHA", "PORTUGAL", "M" "\xC3\x89" "XICO", "ARGENTINA",
    "ESTADOS UNIDOS", "REINO UNIDO", "AUSTR" "\xC3\x81" "LIA", "CHINA", "\xC3\x8D" "NDIA",
    "R" "\xC3\x9A" "SSIA", "COREIA DO SUL", "EGITO", "TURQUIA", "GR" "\xC3\x89" "CIA",
    "NORUEGA", "SU" "\xC3\x89" "CIA", "DINAMARCA", "FINL" "\xC3\x82" "NDIA", "HOLANDA",
    "B" "\xC3\x89" "LGICA", "SU" "\xC3\x8D" "\xC3\x87" "A", "\xC3\x81" "USTRIA", "POL" "\xC3\x94" "NIA", "REP" "\xC3\x9A" "BLICA TCHECA",
    "COL" "\xC3\x94" "MBIA", "CHILE", "PERU", "CUBA", "JAMAICA",
    "TAIL" "\xC3\x82" "NDIA", "VIETN" "\xC3\x83", "INDON" "\xC3\x89" "SIA", "MAL" "\xC3\x81" "SIA", "FILIPINAS",
    "IRLANDA", "ESC" "\xC3\x93" "CIA", "ISL" "\xC3\x82" "NDIA", "CRO" "\xC3\x81" "CIA", "ROM" "\xC3\x82" "NIA",
    "MARROCOS", "\xC3\x81" "FRICA DO SUL", "NIG" "\xC3\x89" "RIA", "QU" "\xC3\x8A" "NIA", "GANA",
    "ISRAEL", "EMIRADOS", "NOVA ZEL" "\xC3\x82" "NDIA", "PARAGUAI", "URUGUAI",
};

/* -----------------------------------------------------------------------
 * Categoria 5: CIDADES
 * ----------------------------------------------------------------------- */
static const char *const WORDS_CIDADES[] = {
    "S" "\xC3\x83" "O PAULO", "PARIS", "T" "\xC3\x93" "QUIO", "NOVA YORK", "LONDRES",
    "RIO DE JANEIRO", "ROMA", "BARCELONA", "BERLIM", "AMSTERDAM",
    "LISBOA", "BUENOS AIRES", "DUBAI", "SYDNEY", "LOS ANGELES",
    "MIAMI", "ORLANDO", "CANCUN", "HAVANA", "BANGKOK",
    "PRAGA", "VIENA", "ATENAS", "ISTAMBUL", "CAIRO",
    "MUMBAI", "PEQUIM", "SEUL", "SINGAPURA", "HONG KONG",
    "TORONTO", "VANCOUVER", "CIDADE DO M" "\xC3\x89" "XICO", "LIMA", "SANTIAGO",
    "BRAS" "\xC3\x8D" "LIA", "SALVADOR", "FLORIAN" "\xC3\x93" "POLIS", "CURITIBA", "RECIFE",
    "FORTALEZA", "MANAUS", "BEL" "\xC3\x89" "M", "PORTO ALEGRE", "BELO HORIZONTE",
    "VENEZA", "MIL" "\xC3\x83" "O", "MADRI", "MOSCOU", "DUBLIN",
    "OSLO", "COPENHAGUE", "ESTOCOLMO", "HELSINQUE", "MARRAKECH",
};

/* -----------------------------------------------------------------------
 * Categoria 6: LUGARES
 * ----------------------------------------------------------------------- */
static const char *const WORDS_LUGARES[] = {
    "PRAIA", "SHOPPING", "PARQUE", "HOSPITAL", "ESCOLA",
    "CINEMA", "SUPERMERCADO", "ACADEMIA", "RESTAURANTE", "AEROPORTO",
    "BIBLIOTECA", "MUSEU", "IGREJA", "EST" "\xC3\x81" "DIO", "TEATRO",
    "PADARIA", "FARM" "\xC3\x81" "CIA", "DELEGACIA", "CORREIOS", "BANCO",
    "ZOOL" "\xC3\x93" "GICO", "AQU" "\xC3\x81" "RIO", "PLANET" "\xC3\x81" "RIO", "CIRCO", "BALADA",
    "CHURRASCARIA", "PIZZARIA", "SORVETERIA", "LANCHONETE", "BOTECO",
    "MERCADO", "FEIRA", "PRA" "\xC3\x87" "A", "JARDIM", "MIRANTE",
    "PONTE", "FAROL", "PORTO", "ESTA" "\xC3\x87" "\xC3\x83" "O DE TREM", "METR" "\xC3\x94",
    "PISCINA", "CAMPO DE FUTEBOL", "QUADRA", "PISTA DE CORRIDA", "MONTANHA",
    "CAVERNA", "CACHOEIRA", "LAGO", "RIO", "FLORESTA",
    "DESERTO", "ILHA", "VULC" "\xC3\x83" "O", "CASTELO", "RU" "\xC3\x8D" "NAS",
};

/* -----------------------------------------------------------------------
 * Categoria 7: OBJETOS
 * ----------------------------------------------------------------------- */
static const char *const WORDS_OBJETOS[] = {
    "CHAVE", "\xC3\x93" "CULOS", "REL" "\xC3\x93" "GIO", "ESPELHO", "GUARDA-CHUVA",
    "MOCHILA", "CARTEIRA", "CANETA", "TESOURA", "LANTERNA",
    "GARRAFA", "COPO", "PRATO", "GARFO", "FACA",
    "PANELA", "FRIGIDEIRA", "BALDE", "VASSOURA", "ESCADA",
    "MARTELO", "CHAVE DE FENDA", "ALICATE", "SERROTE", "FURADEIRA",
    "LIVRO", "CADERNO", "MAPA", "GLOBO", "CALCULADORA",
    "COFRE", "CADEADO", "SINO", "VELA", "ISQUEIRO",
    "DADO", "BARALHO", "BOLA", "SKATE", "PATINS",
    "VIOL" "\xC3\x83" "O", "PIANO", "FLAUTA", "BATERIA", "MICROFONE",
    "C" "\xC3\x82" "MERA", "BIN" "\xC3\x93" "CULOS", "B" "\xC3\x9A" "SSOLA", "TERM" "\xC3\x94" "METRO", "LUPA",
    "APITO", "CAPACETE", "EXTINTOR", "BANDEIRA", "TROF" "\xC3\x89" "U",
};

/* -----------------------------------------------------------------------
 * Categoria 8: PROFISSÕES
 * ----------------------------------------------------------------------- */
static const char *const WORDS_PROFISSOES[] = {
    "M" "\xC3\x89" "DICO", "PROFESSOR", "ENGENHEIRO", "BOMBEIRO", "POLICIAL",
    "ADVOGADO", "PILOTO", "ASTRONAUTA", "COZINHEIRO", "PADEIRO",
    "DENTISTA", "VETERINARIO", "ENFERMEIRO", "FARMACEUTICO", "PSICOLOGO",
    "ARQUITETO", "ELETRICISTA", "ENCANADOR", "MEC" "\xC3\x82" "NICO", "MOTORISTA",
    "JORNALISTA", "FOT" "\xC3\x93" "GRAFO", "ATOR", "DIRETOR", "PRODUTOR",
    "CANTOR", "M" "\xC3\x9A" "SICO", "DAN" "\xC3\x87" "ARINO", "ESCRITOR", "POETA",
    "PINTOR", "ESCULTOR", "DESIGNER", "PROGRAMADOR", "CIENTISTA",
    "BI" "\xC3\x93" "LOGO", "QU" "\xC3\x8D" "MICO", "F" "\xC3\x8D" "SICO", "MATEM" "\xC3\x81" "TICO", "GE" "\xC3\x93" "LOGO",
    "JUIZ", "DETETIVE", "ESPI" "\xC3\x83" "O", "SOLDADO", "GENERAL",
    "JARDINEIRO", "PESCADOR", "AGRICULTOR", "LENHADOR", "MERGULHADOR",
    "GAR" "\xC3\x87" "OM", "BARBEIRO", "CARTEIRO", "BIBLIOTEC" "\xC3\x81" "RIO", "TRADUTOR",
};

/* -----------------------------------------------------------------------
 * Categoria 9: ESPORTES
 * ----------------------------------------------------------------------- */
static const char *const WORDS_ESPORTES[] = {
    "FUTEBOL", "BASQUETE", "NATA" "\xC3\x87" "\xC3\x83" "O", "SURF", "T" "\xC3\x8A" "NIS",
    "V" "\xC3\x94" "LEI", "HANDEBOL", "BEISEBOL", "GOLFE", "BOXE",
    "JUD" "\xC3\x94", "KARAT" "\xC3\x8A", "ESGRIMA", "ATLETISMO", "CICLISMO",
    "SKATE", "PATINA" "\xC3\x87" "\xC3\x83" "O", "HIPISMO", "POLO AQU" "\xC3\x81" "TICO", "REMO",
    "VELA", "CANOAGEM", "ESCALADA", "ALPINISMO", "PARKOUR",
    "RUGBY", "CRICKET", "H" "\xC3\x93" "QUEI", "CURLING", "BOLICHE",
    "BILHAR", "DARDOS", "XADREZ", "AUTOMOBILISMO", "MOTOCROSS",
    "F" "\xC3\x93" "RMULA 1", "RALLY", "MARATONA", "TRIATLO", "CROSSFIT",
    "CAPOEIRA", "MMA", "WRESTLING", "TIRO COM ARCO", "PENTATLO",
    "BADMINTON", "PING PONG", "POLO", "LACROSSE", "SOFTBALL",
    "MERGULHO", "WINDSURF", "KITESURF", "WAKEBOARD", "SNOWBOARD",
};

/* -----------------------------------------------------------------------
 * Categoria 10: PARTES DO CORPO
 * ----------------------------------------------------------------------- */
static const char *const WORDS_CORPO[] = {
    "M" "\xC3\x83" "O", "OLHO", "JOELHO", "COTOVELO", "CABE" "\xC3\x87" "A",
    "P" "\xC3\x89", "ORELHA", "NARIZ", "BOCA", "DEDO",
    "BRA" "\xC3\x87" "O", "PERNA", "OMBRO", "PESCO" "\xC3\x87" "O", "COSTA",
    "BARRIGA", "PEITO", "QUADRIL", "TORNOZELO", "PULSO",
    "COXA", "PANTURRILHA", "CALCANHAR", "QUEIXO", "TESTA",
    "SOBRANCELHA", "C" "\xC3\x8D" "LIOS", "L" "\xC3\x81" "BIO", "L" "\xC3\x8D" "NGUA", "DENTE",
    "UNHA", "CABELO", "CINTURA", "CANELA", "POLEGAR",
    "INDICADOR", "MINDINHO", "PALMA", "NUCA", "AXILA",
    "CORA" "\xC3\x87" "\xC3\x83" "O", "PULM" "\xC3\x83" "O", "F" "\xC3\x8D" "GADO", "EST" "\xC3\x94" "MAGO", "C" "\xC3\x89" "REBRO",
    "RIM", "INTESTINO", "COLUNA", "COSTELA", "CLAV" "\xC3\x8D" "CULA",
    "F" "\xC3\x8A" "MUR", "MAND" "\xC3\x8D" "BULA", "RETINA", "TRAQUEIA", "DIAFRAGMA",
};

/* -----------------------------------------------------------------------
 * Categoria 11: HERÓIS (Marvel + DC misturados)
 * ----------------------------------------------------------------------- */
static const char *const WORDS_HEROIS[] = {
    "HOMEM-ARANHA", "BATMAN", "SUPERMAN", "MULHER-MARAVILHA", "HULK",
    "FLASH", "THOR", "LANTERNA VERDE", "CAPIT" "\xC3\x83" "O AM" "\xC3\x89" "RICA", "HOMEM DE FERRO",
    "AQUAMAN", "GAVI" "\xC3\x83" "O ARQUEIRO", "VI" "\xC3\x9A" "VA NEGRA", "PANTERA NEGRA", "WOLVERINE",
    "DOUTOR ESTRANHO", "FALC" "\xC3\x83" "O", "VIS" "\xC3\x83" "O", "FEITICEIRA ESCARLATE", "ANT-MAN",
    "DEADPOOL", "GAMBIT", "TEMPESTADE", "CICLOPE", "JEAN GREY",
    "PROFESSOR X", "MAGNETO", "NOTURNO", "COLOSSUS", "VAMPIRA",
    "SHAZAM", "SUPERGIRL", "BATGIRL", "CIBORGUE", "ARQUEIRO VERDE",
    "CAN" "\xC3\x81" "RIO NEGRO", "JOHN CONSTANTINE", "RORSCHACH", "DEMOLIDOR", "LUKE CAGE",
    "JESSICA JONES", "PUNHO DE FERRO", "SENHOR DESTINO", "AJAX", "M" "\xC3\x8D" "STICA",
    "CAPIT" "\xC3\x83" " MARVEL", "HOMEM-FORMIGA", "VESPA", "FALC" "\xC3\x83" "O NOTURNO", "ROBIN",
    "BLADE", "HELLBOY", "ROCKET", "GROOT", "GAMORA",
    "STAR-LORD", "DRAX", "NEBULOSA", "ADAM WARLOCK", "MILES MORALES",
};

/* -----------------------------------------------------------------------
 * Categoria 12: FILMES
 * ----------------------------------------------------------------------- */
static const char *const WORDS_FILMES[] = {
    "TITANIC", "MATRIX", "AVATAR", "FROZEN", "SHREK",
    "PROCURANDO NEMO", "TOY STORY", "REI LE" "\xC3\x83" "O", "JURASSIC PARK", "HARRY POTTER",
    "SENHOR DOS AN" "\xC3\x89" "IS", "VINGADORES", "STAR WARS", "BATMAN", "HOMEM-ARANHA",
    "INTERESTELAR", "INCEPTION", "FORREST GUMP", "GLADIADOR", "CORALINE",
    "UP", "WALL-E", "RATATOUILLE", "COCO", "MOANA",
    "ALADDIN", "MULAN", "TARZAN", "BAMBI", "DUMBO",
    "CORINGA", "PANTERA NEGRA", "GUARDI" "\xC3\x95" "ES DA GAL" "\xC3\x81" "XIA", "HOMEM DE FERRO", "THOR",
    "DE VOLTA PARA O FUTURO", "E.T.", "TUBAR" "\xC3\x83" "O", "INDIANA JONES", "ROCKY",
    "RAMBO", "TERMINATOR", "ALIEN", "PREDADOR", "ROBOCOP",
    "MAD MAX", "TOP GUN", "MISS" "\xC3\x83" "O IMPOSS" "\xC3\x8D" "VEL", "VELOZES E FURIOSOS", "JOHN WICK",
    "A ORIGEM", "O PODEROSO CHEF" "\xC3\x83" "O", "CLUBE DA LUTA", "BASTARDOS INGL" "\xC3\x93" "RIOS", "DJANGO",
    "ENCANTO", "DIVERTIDA MENTE", "SOUL", "LUCA", "ELEMENTOS",
};

/* -----------------------------------------------------------------------
 * Categoria 13: SÉRIES
 * ----------------------------------------------------------------------- */
static const char *const WORDS_SERIES[] = {
    "FRIENDS", "BREAKING BAD", "STRANGER THINGS", "GAME OF THRONES", "THE OFFICE",
    "LA CASA DE PAPEL", "NARCOS", "SQUID GAME", "WANDAVISION", "LOKI",
    "MANDALORIAN", "PEAKY BLINDERS", "VIKINGS", "THE WITCHER", "SHERLOCK",
    "MR. ROBOT", "BLACK MIRROR", "DARK", "CHERNOBYL", "BAND OF BROTHERS",
    "LOST", "PRISON BREAK", "WALKING DEAD", "DEXTER", "HOUSE",
    "GREY'S ANATOMY", "SUITS", "HOW I MET YOUR MOTHER", "BIG BANG THEORY", "SEINFELD",
    "THE BOYS", "INVINCIBLE", "ARCANE", "ONE PIECE", "ATTACK ON TITAN",
    "NARUTO", "DRAGON BALL", "DEMON SLAYER", "JUJUTSU KAISEN", "SPY X FAMILY",
    "THE LAST OF US", "SUCCESSION", "EUPHORIA", "WEDNESDAY", "LUPIN",
    "ROUND 6", "COBRA KAI", "YELLOWSTONE", "OZARK", "BETTER CALL SAUL",
    "SEVERANCE", "TED LASSO", "BRIDGERTON", "SANDMAN", "RINGS OF POWER",
};

/* -----------------------------------------------------------------------
 * Categoria 14: JOGOS
 * ----------------------------------------------------------------------- */
static const char *const WORDS_JOGOS[] = {
    "MINECRAFT", "XADREZ", "UNO", "MARIO KART", "FORTNITE",
    "AMONG US", "ROBLOX", "FIFA", "GTA", "ZELDA",
    "POKEMON", "TETRIS", "PACMAN", "SONIC", "SUPER MARIO",
    "GOD OF WAR", "HALO", "CALL OF DUTY", "OVERWATCH", "VALORANT",
    "LEAGUE OF LEGENDS", "DOTA", "COUNTER-STRIKE", "APEX LEGENDS", "PUBG",
    "ELDEN RING", "DARK SOULS", "SKYRIM", "FALLOUT", "THE SIMS",
    "ANIMAL CROSSING", "STARDEW VALLEY", "TERRARIA", "RESIDENT EVIL", "SILENT HILL",
    "CRASH BANDICOOT", "SPYRO", "DONKEY KONG", "KIRBY", "MEGAMAN",
    "STREET FIGHTER", "MORTAL KOMBAT", "TEKKEN", "SMASH BROS", "GUILTY GEAR",
    "BANCO IMOBILI" "\xC3\x81" "RIO", "DETETIVE", "WAR", "LUDO", "DOMIN" "\xC3\x93",
    "BARALHO", "TRUCO", "DAMA", "GAM" "\xC3\x83" "O", "JOGO DA VIDA",
};

/* -----------------------------------------------------------------------
 * Categoria 15: PERSONAGENS
 * ----------------------------------------------------------------------- */
static const char *const WORDS_PERSONAGENS[] = {
    "MICKEY", "SHREK", "HARRY POTTER", "GOKU", "NARUTO",
    "PIKACHU", "BOB ESPONJA", "HOMER SIMPSON", "SCOOBY-DOO", "TOM E JERRY",
    "BUZZ LIGHTYEAR", "WOODY", "NEMO", "SIMBA", "ELSA",
    "CINDERELA", "BRANCA DE NEVE", "RAPUNZEL", "BELA", "ARIEL",
    "PETER PAN", "PIN" "\xC3\x93" "QUIO", "BAMBI", "DUMBO", "ALADDIN",
    "GANDALF", "FRODO", "DARTH VADER", "YODA", "CHEWBACCA",
    "INDIANA JONES", "JAMES BOND", "SHERLOCK HOLMES", "ROBIN HOOD", "ZORRO",
    "LUFFY", "GOLLUM", "THANOS", "VOLDEMORT", "CORINGA",
    "WILLY WONKA", "FORREST GUMP", "JACK SPARROW", "JOHN WICK", "NEO",
    "TOTORO", "ASTRO BOY", "SAILOR MOON", "SAKURA", "VEGETA",
    "CHAPOLIN", "CHAVES", "MAFALDA", "SNOOPY", "GARFIELD",
};

/* -----------------------------------------------------------------------
 * Categoria 16: MARCAS
 * ----------------------------------------------------------------------- */
static const char *const WORDS_MARCAS[] = {
    "NIKE", "APPLE", "COCA-COLA", "GOOGLE", "AMAZON",
    "MICROSOFT", "SAMSUNG", "TOYOTA", "FERRARI", "ADIDAS",
    "MCDONALDS", "STARBUCKS", "NETFLIX", "SPOTIFY", "YOUTUBE",
    "INSTAGRAM", "TIKTOK", "WHATSAPP", "TWITTER", "FACEBOOK",
    "LEGO", "DISNEY", "NINTENDO", "PLAYSTATION", "XBOX",
    "GUCCI", "LOUIS VUITTON", "ZARA", "PUMA", "REEBOK",
    "BMW", "MERCEDES", "AUDI", "PORSCHE", "LAMBORGHINI",
    "ROLEX", "RAY-BAN", "CHANEL", "PRADA", "DIOR",
    "PEPSI", "NESTL" "\xC3\x89", "NUTELLA", "OREO", "DORITOS",
    "UBER", "AIRBNB", "TESLA", "SPACEX", "NASA",
    "HAVAIANAS", "NATURA", "AMBEV", "GLOBO", "PETROBRAS",
};

/* -----------------------------------------------------------------------
 * Categoria 17: VEÍCULOS
 * ----------------------------------------------------------------------- */
static const char *const WORDS_VEICULOS[] = {
    "CARRO", "HELIC" "\xC3\x93" "PTERO", "BICICLETA", "TREM", "NAVIO",
    "AVI" "\xC3\x83" "O", "MOTO", "\xC3\x94" "NIBUS", "METR" "\xC3\x94", "BONDE",
    "BARCO", "VELEIRO", "LANCHA", "IATE", "SUBMARINO",
    "FOGUETE", "PATINETE", "SKATE", "TRICICLO", "QUADRICICLO",
    "CAMINH" "\xC3\x83" "O", "VAN", "AMBUL" "\xC3\x82" "NCIA", "BOMBEIRO", "TRATOR",
    "ESCAVADEIRA", "EMPILHADEIRA", "JIPE", "LIMUSINE", "T" "\xC3\x81" "XI",
    "UBER", "CANOA", "CAIAQUE", "BALSA", "CRUZEIRO",
    "PARAGLIDER", "DIRIG" "\xC3\x8D" "VEL", "BAL" "\xC3\x83" "O", "TELEF" "\xC3\x89" "RICO", "MONOTRILHO",
    "SEGWAY", "HOVERBOARD", "JET SKI", "BUGGY", "KART",
    "CHARRETE", "TREN" "\xC3\x93", "CARRO" "\xC3\x87" "A", "G" "\xC3\x94" "NDOLA", "BALSINHA",
    "PICK-UP", "CONVERS" "\xC3\x8D" "VEL", "SUV", "KOMBI", "FUSCA",
};

/* -----------------------------------------------------------------------
 * Categoria 18: TECNOLOGIA
 * ----------------------------------------------------------------------- */
static const char *const WORDS_TECNOLOGIA[] = {
    "CELULAR", "COMPUTADOR", "DRONE", "BLUETOOTH", "WI-FI",
    "TABLET", "NOTEBOOK", "SMARTWATCH", "FONE DE OUVIDO", "CAIXA DE SOM",
    "IMPRESSORA", "SCANNER", "WEBCAM", "PENDRIVE", "HD EXTERNO",
    "MONITOR", "TECLADO", "MOUSE", "CONTROLE", "PROJETOR",
    "GPS", "ALEXA", "SIRI", "CHATGPT", "ROB" "\xC3\x94",
    "INTELIG" "\xC3\x8A" "NCIA ARTIFICIAL", "REALIDADE VIRTUAL", "REALIDADE AUMENTADA", "HOLOGRAFIA", "5G",
    "BITCOIN", "BLOCKCHAIN", "NFT", "STREAMING", "PODCAST",
    "PIXEL", "ALGORITMO", "APLICATIVO", "REDE SOCIAL", "NUVEM",
    "SERVIDOR", "FIREWALL", "ANTIVIRUS", "HACKER", "CRIPTOGRAFIA",
    "QR CODE", "RECONHECIMENTO FACIAL", "SENSOR", "MICROCHIP", "FIBRA " "\xC3\x93" "PTICA",
    "SAT" "\xC3\x89" "LITE", "TELESC" "\xC3\x93" "PIO", "MICROSC" "\xC3\x93" "PIO", "LASER", "LED",
};

/* -----------------------------------------------------------------------
 * Categoria 19: NATUREZA
 * ----------------------------------------------------------------------- */
static const char *const WORDS_NATUREZA[] = {
    "CACHOEIRA", "VULC" "\xC3\x83" "O", "ARCO-" "\xC3\x8D" "RIS", "AURORA BOREAL", "TERREMOTO",
    "FURAC" "\xC3\x83" "O", "TORNADO", "TSUNAMI", "RAIO", "REL" "\xC3\x82" "MPAGO",
    "SOL", "LUA", "ESTRELA", "COMETA", "METEORO",
    "\xC3\x81" "RVORE", "FLOR", "COGUMELO", "CACTO", "SAMAMBAIA",
    "MONTANHA", "VALE", "CANYON", "GELEIRA", "ICEBERG",
    "OCEANO", "RIO", "LAGO", "LAGOA", "P" "\xC3\x82" "NTANO",
    "DESERTO", "SAVANA", "TUNDRA", "FLORESTA", "SELVA",
    "CORAL", "RECIFE", "GRUTA", "CAVERNA", "CRATERA",
    "DIAMANTE", "OURO", "CRISTAL", "\xC3\x82" "MBAR", "PEDRA",
    "AREIA", "ARGILA", "GRANITO", "M" "\xC3\x81" "RMORE", "QUARTZO",
    "ORVALHO", "NEBLINA", "GRANIZO", "NEVE", "GEADA",
};

/* -----------------------------------------------------------------------
 * Categoria 20: CASA
 * ----------------------------------------------------------------------- */
static const char *const WORDS_CASA[] = {
    "SOF" "\xC3\x81", "GELADEIRA", "CHUVEIRO", "TRAVESSEIRO", "COBERTOR",
    "CAMA", "MESA", "CADEIRA", "ABAJUR", "CORTINA",
    "TAPETE", "QUADRO", "VASO", "ESTANTE", "GUARDA-ROUPA",
    "ESPELHO", "VENTILADOR", "AR-CONDICIONADO", "AQUECEDOR", "UMIDIFICADOR",
    "FOG" "\xC3\x83" "O", "MICRO-ONDAS", "TORRADEIRA", "CAFETEIRA", "LIQUIDIFICADOR",
    "BATEDEIRA", "MIXER", "ASPIRADOR", "FERRO DE PASSAR", "M" "\xC3\x81" "QUINA DE LAVAR",
    "SECADORA", "VARAL", "LIXEIRA", "RODO", "PANO DE CH" "\xC3\x83" "O",
    "TOALHA", "SABONETE", "SHAMPOO", "ESCOVA DE DENTE", "PASTA DE DENTE",
    "PAPEL HIGI" "\xC3\x8A" "NICO", "DESCARGA", "PIA", "TORNEIRA", "BANHEIRA",
    "CHURRASQUEIRA", "REDE", "BANQUETA", "CRIADO-MUDO", "POLTRONA",
    "LUSTRE", "INTERRUPTOR", "TOMADA", "FECHADURA", "CAMPAINHA",
};

/* -----------------------------------------------------------------------
 * Tabela global de categorias
 * ----------------------------------------------------------------------- */

const fora_category_t FORA_CATEGORIES[FORA_CATEGORY_COUNT] = {
    { "COMIDAS",                              WORDS_COMIDAS,      COUNTOF(WORDS_COMIDAS)      },
    { "BEBIDAS",                              WORDS_BEBIDAS,      COUNTOF(WORDS_BEBIDAS)      },
    { "ANIMAIS",                              WORDS_ANIMAIS,      COUNTOF(WORDS_ANIMAIS)      },
    { "PA" "\xC3\x8D" "SES",                  WORDS_PAISES,       COUNTOF(WORDS_PAISES)       },
    { "CIDADES",                              WORDS_CIDADES,      COUNTOF(WORDS_CIDADES)      },
    { "LUGARES",                              WORDS_LUGARES,      COUNTOF(WORDS_LUGARES)      },
    { "OBJETOS",                              WORDS_OBJETOS,      COUNTOF(WORDS_OBJETOS)      },
    { "PROFISS" "\xC3\x95" "ES",              WORDS_PROFISSOES,   COUNTOF(WORDS_PROFISSOES)   },
    { "ESPORTES",                             WORDS_ESPORTES,     COUNTOF(WORDS_ESPORTES)     },
    { "CORPO",                                WORDS_CORPO,        COUNTOF(WORDS_CORPO)        },
    { "HER" "\xC3\x93" "IS",                  WORDS_HEROIS,       COUNTOF(WORDS_HEROIS)       },
    { "FILMES",                               WORDS_FILMES,       COUNTOF(WORDS_FILMES)       },
    { "S" "\xC3\x89" "RIES",                  WORDS_SERIES,       COUNTOF(WORDS_SERIES)       },
    { "JOGOS",                                WORDS_JOGOS,        COUNTOF(WORDS_JOGOS)        },
    { "PERSONAGENS",                          WORDS_PERSONAGENS,  COUNTOF(WORDS_PERSONAGENS)  },
    { "MARCAS",                               WORDS_MARCAS,       COUNTOF(WORDS_MARCAS)       },
    { "VE" "\xC3\x8D" "CULOS",               WORDS_VEICULOS,     COUNTOF(WORDS_VEICULOS)     },
    { "TECNOLOGIA",                           WORDS_TECNOLOGIA,   COUNTOF(WORDS_TECNOLOGIA)   },
    { "NATUREZA",                             WORDS_NATUREZA,     COUNTOF(WORDS_NATUREZA)     },
    { "CASA",                                 WORDS_CASA,         COUNTOF(WORDS_CASA)         },
};
