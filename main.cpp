#include <string>
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <iomanip> // Para evitar la notación científica (e+06)
#include <random>
using namespace std;
mt19937 rng(67); 

// =========================================================================
// ESTRUCTURAS DE DATOS
// =========================================================================

struct Deposito {
    int id;
    double capacidad_maxima;      
    double capacidad_usada = 0.0;   
    vector<int> vendedores_asignados; 
};

struct Vendedor {
    int id;
    int deposito_asignado = -1; // -1 indica no asignado (Depósito Ficticio)
};

struct InstanciaGAP {
    int n_vendedores;
    int m_depositos;
    vector<Deposito> depositos;
    vector<Vendedor> vendedores;
    vector<vector<double>> costos;   // Matriz c_{ij}
    vector<vector<double>> demandas; // Matriz d_{ij}
};

// =========================================================================
// PARSER UNIVERSAL
// =========================================================================

InstanciaGAP cargar_instancia(const std::string& ruta_archivo) {
    ifstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        cerr << "Error crítico: No se pudo abrir el archivo " << ruta_archivo << endl;
        exit(EXIT_FAILURE);
    }

    InstanciaGAP instancia;
    archivo >> instancia.m_depositos >> instancia.n_vendedores;

    instancia.depositos.resize(instancia.m_depositos);
    instancia.vendedores.resize(instancia.n_vendedores);
    instancia.costos.assign(instancia.m_depositos, vector<double>(instancia.n_vendedores, 0.0));
    instancia.demandas.assign(instancia.m_depositos, vector<double>(instancia.n_vendedores, 0.0));

    for (int i = 0; i < instancia.m_depositos; ++i) instancia.depositos[i].id = i;
    for (int j = 0; j < instancia.n_vendedores; ++j) instancia.vendedores[j].id = j;

    for (int i = 0; i < instancia.m_depositos; ++i) {
        for (int j = 0; j < instancia.n_vendedores; ++j) {
            archivo >> instancia.costos[i][j];
        }
    }

    for (int i = 0; i < instancia.m_depositos; ++i) {
        for (int j = 0; j < instancia.n_vendedores; ++j) {
            archivo >> instancia.demandas[i][j];
        }
    }

    for (int i = 0; i < instancia.m_depositos; ++i) {
        archivo >> instancia.depositos[i].capacidad_maxima;
        instancia.depositos[i].capacidad_usada = 0.0;
    }

    archivo.close();
    return instancia;
}

// =========================================================================
// FUNCIONES DE EVALUACION
// =========================================================================

double calcular_funcion_objetivo(const InstanciaGAP& instancia, bool silencioso = false) {
    double costo_total = 0.0;
    double c_max = 0.0;
    int vendedores_no_asignados = 0;

    for (int i = 0; i < instancia.m_depositos; ++i) {
        for (int j = 0; j < instancia.n_vendedores; ++j) {
            if (instancia.costos[i][j] > c_max) c_max = instancia.costos[i][j];
        }
    }

    for (int j = 0; j < instancia.n_vendedores; ++j) {
        int i = instancia.vendedores[j].deposito_asignado;
        if (i != -1) {
            costo_total += instancia.costos[i][j];
        } else {
            vendedores_no_asignados++;
            costo_total += 3.0 * c_max; 
        }
    }

    if (!silencioso && vendedores_no_asignados > 0) {
        cout << "   [!] Solucion parcial. Vendedores sin asignar: " << vendedores_no_asignados << endl;
    }

    return costo_total;
}

double obtener_multa(const InstanciaGAP& instancia) {
    double c_max = 0.0;
    for (int i = 0; i < instancia.m_depositos; ++i) {
        for (int j = 0; j < instancia.n_vendedores; ++j) {
            if (instancia.costos[i][j] > c_max) c_max = instancia.costos[i][j];
        }
    }
    return 3.0 * c_max;
}

// =========================================================================
// HEURISTICAS CONSTRUCTIVAS
// =========================================================================

void heuristica_constructiva_greedy(InstanciaGAP& instancia) {
    for (int j = 0; j < instancia.n_vendedores; ++j) {
        int mejor_deposito = -1;
        double menor_costo = numeric_limits<double>::max();

        for (int i = 0; i < instancia.m_depositos; ++i) {
            double costo_actual = instancia.costos[i][j];
            double demanda_ij = instancia.demandas[i][j];
            double espacio_libre = instancia.depositos[i].capacidad_maxima - instancia.depositos[i].capacidad_usada;

            if (espacio_libre >= demanda_ij && costo_actual < menor_costo) {
                menor_costo = costo_actual;
                mejor_deposito = i;
            }
        }

        if (mejor_deposito != -1) {
            instancia.vendedores[j].deposito_asignado = mejor_deposito;
            instancia.depositos[mejor_deposito].vendedores_asignados.push_back(j);
            instancia.depositos[mejor_deposito].capacidad_usada += instancia.demandas[mejor_deposito][j];
        }
    }
}

void heuristica_constructiva_demanda(InstanciaGAP& instancia) {
    vector<int> indices(instancia.n_vendedores);
    iota(indices.begin(), indices.end(), 0);

    vector<double> peso_estimado_total(instancia.n_vendedores, 0.0);
    for(int j = 0; j < instancia.n_vendedores; ++j) {
        for(int i = 0; i < instancia.m_depositos; ++i) {
            peso_estimado_total[j] += instancia.demandas[i][j];
        }
    }

    sort(indices.begin(), indices.end(), [&](int a, int b) {
        return peso_estimado_total[a] > peso_estimado_total[b];
    });

    for (int j : indices) {
        int mejor_deposito = -1;
        double menor_costo = numeric_limits<double>::max();

        for (int i = 0; i < instancia.m_depositos; ++i) {
            double costo_actual = instancia.costos[i][j];
            double demanda_ij = instancia.demandas[i][j];
            double espacio_libre = instancia.depositos[i].capacidad_maxima - instancia.depositos[i].capacidad_usada;

            if (espacio_libre >= demanda_ij && costo_actual < menor_costo) {
                menor_costo = costo_actual;
                mejor_deposito = i;
            }
        }

        if (mejor_deposito != -1) {
            instancia.vendedores[j].deposito_asignado = mejor_deposito;
            instancia.depositos[mejor_deposito].vendedores_asignados.push_back(j);
            instancia.depositos[mejor_deposito].capacidad_usada += instancia.demandas[mejor_deposito][j];
        }
    }
}

// =========================================================================
// BUSQUEDA LOCAL
// =========================================================================

bool intentar_reubicar_un_vendedor(InstanciaGAP& instancia, double costo_multa) {
    bool mejora_global = false;
    for (int j = 0; j < instancia.n_vendedores; ++j) {
        int origen = instancia.vendedores[j].deposito_asignado;
        double costo_actual = (origen == -1) ? costo_multa : instancia.costos[origen][j];

        int mejor_deposito = origen;
        double mejor_costo = costo_actual;

        for (int destino = 0; destino < instancia.m_depositos; ++destino) {
            if (destino == origen) continue;

            double demanda_en_destino = instancia.demandas[destino][j];
            double espacio_libre = instancia.depositos[destino].capacidad_maxima - instancia.depositos[destino].capacidad_usada;

            if (espacio_libre >= demanda_en_destino && instancia.costos[destino][j] < mejor_costo) {
                mejor_costo = instancia.costos[destino][j];
                mejor_deposito = destino;
            }
        }

        if (mejor_deposito != origen) {
            if (origen != -1) {
                instancia.depositos[origen].capacidad_usada -= instancia.demandas[origen][j];
                auto& vec_origen = instancia.depositos[origen].vendedores_asignados;
                vec_origen.erase(remove(vec_origen.begin(), vec_origen.end(), j), vec_origen.end());
            }

            instancia.vendedores[j].deposito_asignado = mejor_deposito;
            instancia.depositos[mejor_deposito].capacidad_usada += instancia.demandas[mejor_deposito][j];
            instancia.depositos[mejor_deposito].vendedores_asignados.push_back(j);
            mejora_global = true;
        }
    }
    return mejora_global;
}

bool operador_swap(InstanciaGAP& instancia) {
    bool mejora_global = false;

    for (int j1 = 0; j1 < instancia.n_vendedores; ++j1) {
        int a = instancia.vendedores[j1].deposito_asignado;
        if (a == -1) continue;  

        for (int j2 = j1 + 1; j2 < instancia.n_vendedores; ++j2) {
            int b = instancia.vendedores[j2].deposito_asignado;
            if (b == -1 || a == b) continue;  

            double nueva_carga_a = instancia.depositos[a].capacidad_usada - instancia.demandas[a][j1] + instancia.demandas[a][j2];
            double nueva_carga_b = instancia.depositos[b].capacidad_usada - instancia.demandas[b][j2] + instancia.demandas[b][j1];

            if (nueva_carga_a > instancia.depositos[a].capacidad_maxima) continue;
            if (nueva_carga_b > instancia.depositos[b].capacidad_maxima) continue;

            double costo_actual = instancia.costos[a][j1] + instancia.costos[b][j2];
            double costo_nuevo  = instancia.costos[b][j1] + instancia.costos[a][j2];

            if (costo_nuevo < costo_actual - 0.0001) {
                instancia.vendedores[j1].deposito_asignado = b;
                instancia.vendedores[j2].deposito_asignado = a;

                instancia.depositos[a].capacidad_usada = nueva_carga_a;
                instancia.depositos[b].capacidad_usada = nueva_carga_b;

                auto& vec_a = instancia.depositos[a].vendedores_asignados;
                auto& vec_b = instancia.depositos[b].vendedores_asignados;

                vec_a.erase(remove(vec_a.begin(), vec_a.end(), j1), vec_a.end());
                vec_b.erase(remove(vec_b.begin(), vec_b.end(), j2), vec_b.end());
                vec_b.push_back(j1);
                vec_a.push_back(j2);

                mejora_global = true;
                break; // Corta el bucle porque j1 ya se mudó
            }
        }
    }
    return mejora_global;
}

// Para aislar Relocate
void busqueda_local_relocate(InstanciaGAP& instancia) {
    double multa = obtener_multa(instancia);
    while (intentar_reubicar_un_vendedor(instancia, multa)) {}
}

// Para aislar Swap
void busqueda_local_swap(InstanciaGAP& instancia) {
    while (operador_swap(instancia)) {}
}

// El bucle que los combina
void busqueda_local_completa(InstanciaGAP& instancia) {
    double multa = obtener_multa(instancia);
    bool mejoro = true;
    while (mejoro) {
        bool m_swap = operador_swap(instancia);
        bool m_relocate = intentar_reubicar_un_vendedor(instancia, multa);
        mejoro = m_swap || m_relocate;
    }
}

// =========================================================================
// METAHEURISTICA: ILS
// =========================================================================

vector<int> seleccionar_k_vendedores_aleatorios(int k, const InstanciaGAP& solucion) {
    vector<int> indices(solucion.n_vendedores);
    iota(indices.begin(), indices.end(), 0);
    shuffle(indices.begin(), indices.end(), rng);
    return vector<int>(indices.begin(), indices.begin() + k);
}

InstanciaGAP perturbar(int k, InstanciaGAP solucion) {
    vector<int> elegidos = seleccionar_k_vendedores_aleatorios(k, solucion);

    // Desasignamos a los elegidos
    for (int id : elegidos) {
        int deposito_actual = solucion.vendedores[id].deposito_asignado;

        if (deposito_actual != -1) {
            solucion.depositos[deposito_actual].capacidad_usada -= solucion.demandas[deposito_actual][id];
            auto& vec_origen = solucion.depositos[deposito_actual].vendedores_asignados;
            vec_origen.erase(remove(vec_origen.begin(), vec_origen.end(), id), vec_origen.end());
        }
        solucion.vendedores[id].deposito_asignado = -1;
    }

    // Mezclamos para darles un nuevo orden aleatorio de inserción
    shuffle(elegidos.begin(), elegidos.end(), rng);

    // Los intentamos reinsertar (Goloso)
    for (int id : elegidos) {
        int mejor_deposito = -1;
        double menor_costo = numeric_limits<double>::max();
        for (int i = 0; i < solucion.m_depositos; ++i) {
            double espacio_libre = solucion.depositos[i].capacidad_maxima - solucion.depositos[i].capacidad_usada;
            if (espacio_libre >= solucion.demandas[i][id] && solucion.costos[i][id] < menor_costo) {
                menor_costo = solucion.costos[i][id];
                mejor_deposito = i;
            }
        }
        if (mejor_deposito != -1) {
            solucion.vendedores[id].deposito_asignado = mejor_deposito;
            solucion.depositos[mejor_deposito].capacidad_usada += solucion.demandas[mejor_deposito][id];
            solucion.depositos[mejor_deposito].vendedores_asignados.push_back(id);
        }
        // Si no entra en ningun lado, queda en -1 (se encarga la busqueda local o la multa)
    }
    return solucion;
}

InstanciaGAP ILS(InstanciaGAP solucion_inicial, int max_iteraciones, int k) {
    InstanciaGAP mejor_solucion = solucion_inicial;
    InstanciaGAP solucion_actual = perturbar(k, mejor_solucion);

    for (int iter = 0; iter < max_iteraciones; ++iter) {
        busqueda_local_completa(solucion_actual);
        
        if (calcular_funcion_objetivo(solucion_actual, true) < calcular_funcion_objetivo(mejor_solucion, true)) {
            mejor_solucion = solucion_actual;
        }
        solucion_actual = perturbar(k, mejor_solucion);
    }
    return mejor_solucion;
}

// =========================================================================
// EXPORTAR SOLUCION Y MAIN
// =========================================================================

void exportar_solucion(const InstanciaGAP& instancia, const string& ruta_archivo) {
    ofstream archivo(ruta_archivo);
    for (int i = 0; i < instancia.m_depositos; ++i) {
        const auto& vendedores = instancia.depositos[i].vendedores_asignados;
        for (size_t k = 0; k < vendedores.size(); ++k) {
            archivo << vendedores[k] << (k < vendedores.size() - 1 ? " " : "");
        }
        archivo << "\n";
    }
    archivo.close();
}

void imprimir_resumen_asignacion(const InstanciaGAP& instancia) {
    int no_asignados = 0;
    int depositos_usados = 0;
    
    cout << "\n=== RESUMEN DE ASIGNACION (MUESTRA) ===" << endl;
    for (int i = 0; i < instancia.m_depositos; ++i) {
        if (instancia.depositos[i].capacidad_usada > 0) {
            depositos_usados++; 
            cout << "Deposito " << i << " | Usado: " << instancia.depositos[i].capacidad_usada 
             << " / " << instancia.depositos[i].capacidad_maxima 
             << " | Vendedores adentro: " << instancia.depositos[i].vendedores_asignados.size() << endl;
            
        }
    }

    for (int j = 0; j < instancia.n_vendedores; ++j) {
        if (instancia.vendedores[j].deposito_asignado == -1) no_asignados++;
    }
    
    cout << "Total depositos con gente: " << depositos_usados << " / " << instancia.m_depositos << endl;
    cout << "Vendedores no asignados: " << no_asignados << endl;
    cout << "=======================================\n" << endl;
}


int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "Uso: " << argv[0] << " <archivo_input> <archivo_output>" << endl;
        return 1;
    }

    string inputFile = argv[1];
    string outputFile = argv[2];

    cout << fixed << setprecision(1); // Desactiva la notación e+06

    cout << "Leyendo instancia: " << inputFile << "..." << endl;
    InstanciaGAP instancia = cargar_instancia(inputFile);
    cout << "Instancia cargada: M = " << instancia.m_depositos << " | N = " << instancia.n_vendedores << endl;
    cout << "---------------------------------------------------" << endl;

    // ── H1: CLONAMOS LA BASE PARA EVALUAR CADA OPERADOR ──
    InstanciaGAP inst_h1_base = instancia;
    heuristica_constructiva_greedy(inst_h1_base);
    cout << "H1 (Greedy Secuencial)   - PREVIO AL LS:       " << calcular_funcion_objetivo(inst_h1_base, true) << endl;

    InstanciaGAP inst_h1_relocate = inst_h1_base;
    busqueda_local_relocate(inst_h1_relocate);
    cout << "H1 (Greedy Secuencial)   - SOLO RELOCATE:      " << calcular_funcion_objetivo(inst_h1_relocate, true) << endl;

    InstanciaGAP inst_h1_swap = inst_h1_base;
    busqueda_local_swap(inst_h1_swap);
    cout << "H1 (Greedy Secuencial)   - SOLO SWAP:          " << calcular_funcion_objetivo(inst_h1_swap, true) << endl;

    InstanciaGAP inst_h1_completa = inst_h1_base;
    busqueda_local_completa(inst_h1_completa);
    double costo_h1_final = calcular_funcion_objetivo(inst_h1_completa, true);
    cout << "H1 (Greedy Secuencial)   - B.L COMPLETA:       " << costo_h1_final << endl;

    InstanciaGAP inst_h1_ils = ILS(inst_h1_completa, 50, instancia.n_vendedores / 10); // Perturbamos un 10%
    double costo_h1_ils = calcular_funcion_objetivo(inst_h1_ils, false);
    cout << "H1 (Greedy Secuencial)   - METAHEURISTICA ILS: " << costo_h1_ils << endl;

    cout << "---------------------------------------------------" << endl;

    // ── H2: CLONAMOS LA BASE PARA EVALUAR CADA OPERADOR ──
    InstanciaGAP inst_h2_base = instancia;
    heuristica_constructiva_demanda(inst_h2_base);
    cout << "H2 (Demanda Descendente) - PREVIO AL LS:       " << calcular_funcion_objetivo(inst_h2_base, true) << endl;

    InstanciaGAP inst_h2_relocate = inst_h2_base;
    busqueda_local_relocate(inst_h2_relocate);
    cout << "H2 (Demanda Descendente) - SOLO RELOCATE:      " << calcular_funcion_objetivo(inst_h2_relocate, true) << endl;

    InstanciaGAP inst_h2_swap = inst_h2_base;
    busqueda_local_swap(inst_h2_swap);
    cout << "H2 (Demanda Descendente) - SOLO SWAP:          " << calcular_funcion_objetivo(inst_h2_swap, true) << endl;

    InstanciaGAP inst_h2_completa = inst_h2_base;
    busqueda_local_completa(inst_h2_completa);
    double costo_h2_final = calcular_funcion_objetivo(inst_h2_completa, true);
    cout << "H2 (Demanda Descendente) - B.L COMPLETA:       " << costo_h2_final << endl;

    InstanciaGAP inst_h2_ils = ILS(inst_h2_completa, 50, instancia.n_vendedores / 10); // Perturbamos un 10%
    double costo_h2_ils = calcular_funcion_objetivo(inst_h2_ils, false);
    cout << "H2 (Demanda Descendente) - METAHEURISTICA ILS: " << costo_h2_ils << endl;

    cout << "---------------------------------------------------" << endl;

    // Exportar el mejor resultado global entre ambas metaheurísticas
    if (costo_h1_ils <= costo_h2_ils) {
        cout << "Exportando solucion H1_ILS (Mejor costo) a " << outputFile << endl;
        exportar_solucion(inst_h1_ils, outputFile);
    } else {
        cout << "Exportando solucion H2_ILS (Mejor costo) a " << outputFile << endl;
        exportar_solucion(inst_h2_ils, outputFile);
    }
    
    
    // Imprimir la prueba de que el codigo asignó bien
    if (costo_h1_ils <= costo_h2_ils) {
        cout << "Revisando asignacion de H1_ILS..." << endl;
        imprimir_resumen_asignacion(inst_h1_ils);
    } else {
        cout << "Revisando asignacion de H2_ILS..." << endl;
        imprimir_resumen_asignacion(inst_h2_ils);
    }
    
    return 0;
}