#include <iostream>
#include <fstream>
#include <cstring>
#include <queue>
#include <unistd.h>
#include <sys/stat.h>
#include <unordered_set>
#include <pthread.h>
#include <algorithm>

using namespace std;

// Structura folosita pentru a stoca date in coada de prioritate
struct sizeF {
    int size;
    char *file_name;

    sizeF(int size, char *file_name)
        : size(size), file_name(file_name)
    {
    }
};


struct CompareF {
    bool operator() (sizeF const &f1, sizeF const &f2)
    {
        return f1.size < f2.size;
    }
};


// Structura pentru mapperi
struct mapper_str {
    vector<vector<int>> perfect_list;
    priority_queue<sizeF, vector<sizeF>, CompareF> *pq;
    int reducer_count;
    int mapper_count;
    vector<vector<int>> *perfect_powers;
    pthread_mutex_t *mutex1;
    pthread_barrier_t *barrier;
    pthread_mutex_t *mutex2;

    mapper_str(vector<vector<int>> perfect_list, priority_queue<sizeF, 
                vector<sizeF>, CompareF> *pq, int reducer_count,
                int mapper_count, vector<vector<int>> *perfect_powers,
                pthread_mutex_t *mutex1, pthread_barrier_t *barrier, 
                pthread_mutex_t *mutex2)
        : perfect_list(perfect_list), pq(pq), reducer_count(reducer_count), 
        mapper_count(mapper_count), perfect_powers(perfect_powers), mutex1(mutex1),
        barrier(barrier), mutex2(mutex2)
    {
    }

    
};


// Structura pentru reduceri
struct reducer_str{
    vector<mapper_str> *m;
    int pos;
    int mapper_count;
    pthread_barrier_t *barrier;

    reducer_str(vector<mapper_str> *m, int pos, int mapper_count,
                pthread_barrier_t *barrier)
        : m(m), pos(pos), mapper_count(mapper_count), barrier(barrier)
    {
    }
};

// Functia de Map
void *mapper(void *arg){
    ifstream f;
    int nr_numbers, number, i, j;
    unsigned int k;
    mapper_str *m = (mapper_str*)arg;
    vector<int> vec;
    
    while (1) {
        pthread_mutex_lock((*m).mutex1);
        if ((*(*m).pq).empty()) {
            pthread_mutex_unlock((*m).mutex1);
            break;
        }
        f.open((*(*m).pq).top().file_name);
        free((*(*m).pq).top().file_name);
        (*(*m).pq).pop();
        pthread_mutex_unlock((*m).mutex1);

        f>>nr_numbers;
        vector<int> vec(nr_numbers);
        for (i = 0; i < nr_numbers; i++) {
            f>>number;
            vec[i] = number;
        }
        f.close();
        
        // Verific daca numerele sunt puteri perfecte
        for (i = 0; i < nr_numbers; i++) {
            if (vec[i] == 1) {
                pthread_mutex_lock((*m).mutex2);
                for (k = 0; k < ((*m).perfect_list).size(); k++) {
                    ((*m).perfect_list)[k].push_back(vec[i]);
                }
                pthread_mutex_unlock((*m).mutex2);
            } else {
                if (vec[i] > 0) {
                    for (j = 2; j <= (*m).reducer_count + 1; j++) {
                        if (std::binary_search((*(*m).perfect_powers)[j].begin(), 
                        (*(*m).perfect_powers)[j].end(), vec[i])) {
                            pthread_mutex_lock((*m).mutex2);
                            ((*m).perfect_list)[j - 2].push_back(vec[i]);
                            pthread_mutex_unlock((*m).mutex2);
                        }
                    }
                }
            }
        }
    }
    
    pthread_barrier_wait((*m).barrier);
    pthread_exit(NULL);
}


// Functia de Reduce
void *reducer (void *arg) {
    reducer_str r = *(reducer_str*) arg;
    pthread_barrier_wait(r.barrier); 
    unordered_set<int> unique2;
    unsigned int i;
    int j;
    int e = r.pos;

    for (j = 0; j < r.mapper_count; j++) {
        for (i = 0; i < (*r.m)[j].perfect_list[e].size(); i++) {
            unique2.insert((*r.m)[j].perfect_list[e][i]);
        }
    }

    char *outfile = (char *)malloc((e + 10) * sizeof(char));
    strcpy(outfile, "out");
    strcat(outfile, to_string(e + 2).c_str());
    strcat(outfile, ".txt");

    ofstream f(outfile);
    f<<unique2.size();
    f.close();

    free(outfile);
    pthread_exit(NULL);
}


// Procesez inputul
int read_input(long *NUM_THREADS, char **file, int *mapper_count, 
                int *reducer_count, int argc, char *argv[],
                priority_queue<sizeF, vector<sizeF>, CompareF> *pq)
{
    if (argc < 4) {
        std::cout << "Usage:\n\t./tema1 <numar_mapperi> <numar_reduceri> <fisier_intrare>\n";
        return 0;
    }

    sscanf(argv[1], "%d", mapper_count);
    sscanf(argv[2], "%d", reducer_count);
    (*file) = (char*) malloc(strlen(argv[3]) * sizeof(char) + 1);
    strcpy((*file), argv[3]);

    int i, nr_files;
    char *file_name = (char*) malloc(20 * sizeof(char));
    char *aux ;
    ifstream initf, f;

    initf.open((*file));
    initf >> nr_files;
    initf.get();

    for (i = 0; i < nr_files; i++) {
        initf.getline(file_name, 20, '\n');
        aux = (char*) malloc(20 * sizeof(char));
        strcpy(aux, file_name);
        if (*mapper_count > 1) {
            f.open(file_name, ios::in);
            streampos size = f.tellg();
            f.seekg(0, ios::end);
            size = f.tellg() - size;
            (*pq).push(sizeF(sizeof(size), aux));
            f.close();
        } else {
            (*pq).push(sizeF(0, aux));
        }

        
    }

    initf.close();
    free(file_name);
    return 1;
}


int main(int argc, char* argv[]){
    int mapper_count, reducer_count, j, exp, x, nr;
    priority_queue<sizeF, vector<sizeF>, CompareF> pq;
    vector<reducer_str> red;
    vector<vector<int>> perfect_powers;
    char *file = NULL;

    read_input(0,&file,&mapper_count, &reducer_count, argc, argv, &pq);

    perfect_powers.push_back({});
    perfect_powers.push_back({});
    for (j= 2; j <= reducer_count + 1; j++) {
        perfect_powers.push_back({});
        nr = 2;
        while (1){
            x = 1;
            exp = j;

            while(exp != 0) {
                x *= nr;
                --exp;
            }

            if (0 > x) {
                break;
            }

            perfect_powers[j].push_back(x);
            nr++;
        }
    }

    pthread_t threads[mapper_count + reducer_count];
    pthread_mutex_t mutex1;
    pthread_barrier_t barrier;
    pthread_mutex_t mutex2;
    pthread_barrier_init(&barrier, NULL, mapper_count + reducer_count);
    pthread_mutex_init(&mutex1,NULL);
    pthread_mutex_init(&mutex2,NULL);
    
    vector<mapper_str> m(mapper_count, mapper_str(vector<vector<int>>(reducer_count),
    &pq, reducer_count, mapper_count, &perfect_powers, &mutex1, &barrier, &mutex2));

    for (int i = 0; i < reducer_count; i++) {
        red.push_back(reducer_str(&m, i, mapper_count, &barrier));
    }

    // Creez thread-urile
    for (int i = 0; i < mapper_count + reducer_count; i++) {
        if (i < mapper_count) {
            int r = pthread_create(&threads[i], NULL, mapper, (void *) &m[i]);
    
            if (r) {
                printf("Eroare la crearea thread-ului %d\n", i);
                exit(-1);
            }
        } else {
            int r = pthread_create(&threads[i], NULL, reducer, (void *) &red[i-mapper_count]);
            if (r) {
                printf("Eroare la crearea thread-ului %d\n", i);
                exit(-1);
            }
        }
    }
    void *status;
    for (int id = 0; id < mapper_count + reducer_count; id++) {
        int r = pthread_join(threads[id], &status);
 
        if (r) {
            printf("Eroare la asteptarea thread-ului %d\n", id);
            exit(-1);
        }
    }

    pthread_mutex_destroy(&mutex1);
    pthread_mutex_destroy(&mutex2);
    pthread_barrier_destroy(&barrier);
    free(file); 

    return 0;
}