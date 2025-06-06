// Se incluyen las bibliotecas necesarias
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <math.h>
#include <string.h>

// Se incluye lo necesario de la biblioteca a utilizar
#define STB_IMAGE_IMPLEMENTATION
#include "stb-master/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb-master/stb_image_write.h"

#define L 256

// Firmas de los metodos
void HistogramaSecuencial(char *argv[]);
void HistogramaParalelo(char *argv[]);

// Variables para la obtencion de speedup, eficiencia y overhead
float tiemposec = 0, tiempopar = 0, speedup = 0, eficiencia = 0, overhead = 0;
int p;
double tiempos[6];

// Metodo main el cual recibe argumentos
int main(int argc, char *argv[])
{
    // Se obtiene el numero de procesadores
    p = omp_get_num_procs();
    printf("\tResolucion de la imagen\n");
    // Se llaman a ambos metodos
    HistogramaSecuencial(argv);
    HistogramaParalelo(argv);
    // Se imprimen las metricas
    printf("\nNumero de procesadores: %d\n", p);
    // Se obtiene el speedup, eficiencia y overhead
    speedup = tiemposec / tiempopar;
    printf("\nEl speedup es: %f\n", speedup);
    eficiencia = speedup / p;
    printf("La eficiencia es: %f\n", eficiencia);
    overhead = tiempopar - (tiemposec / p);
    printf("Tiempo de Over Head: %f\n", overhead);
    printf("\n\tOtros tiempos Secuencial\n");
    printf("Tiempo de carga de imagen: %f\n", tiempos[0]);
    printf("Tiempo de generacion de imagen: %f\n", tiempos[1]);
    printf("Tiempo de generacion de archivo csv: %f\n", tiempos[2]);
    printf("\n\tOtros tiempos Parallel\n");
    printf("Tiempo de carga de imagen: %f\n", tiempos[3]);
    printf("Tiempo de generacion de imagen: %f\n", tiempos[4]);
    printf("Tiempo de generacion de archivo csv: %f\n", tiempos[5]);
}

// Recibe como parametro el nombre dado desde consola
void HistogramaSecuencial(char *argv[])
{
    // PASO 1
    // Se declaran las variables necesarias
    char *srcPath = argv[1];
    int width, height, channels;

    // Se carga la imagen original y se toman los tiempos
    double tiempocargai = omp_get_wtime();
    unsigned char *srcIma = stbi_load(srcPath, &width, &height, &channels, 0);
    double tiempocargaf = omp_get_wtime();
    double tiempocargaimagen = tiempocargaf - tiempocargai;
    // Se guarda el tiempo en un arreglo
    tiempos[0] = tiempocargaimagen;
    // Se crea lo necesario para darle nombre a los archivos generados
    char *sufijoima = "_eq_secuencial.jpg";
    char *sufijocsv = "_histo_secuencial.csv";
    char nombreimagen[50] = "";
    char nombrecsv[50] = "";
    double tiempoi, tiempof;
    // Se imprimen los valores de la imagen
    printf("Ancho: %d\n", width);
    printf("Alto: %d\n", height);
    printf("Canales: %d\n", channels);
    printf("Tamaño: %d\n", (width * height));

    // Si la imegen no cargo se muestra un mensaje y termina la funcion
    if (srcIma == NULL)
    {
        printf("No se pudo cargar la imagen %s :(\n\n\n", srcPath);
        return;
    }
    // Si la imagen carga se muestran sus datos
    else
    {
        printf("\nImagen cargada correctamente: %dx%d srcIma con %d canales.\n", width, height, channels);
    }
    // Obtiene el tiempo inicial
    tiempoi = omp_get_wtime();

    // PASO 2
    // Se obtiene el histograma de la imagen
    int histo[256], imatam = width * height;
    unsigned char *imaRed = malloc(imatam * channels);
    // Primero se llena de ceros el arreglo para evitar errores
    for (int i = 0; i < 256; i++)
    {
        histo[i] = 0;
    }
    // Si la imagen es de un canal solo se hace una suma
    if (channels == 1)
    {
        for (int i = 0; i < imatam; i++)
        {
            histo[srcIma[i]]++;
        }
    }
    // Si la imagen es de 3 canales se hace un proceso diferente
    if (channels == 3)
    {
        for (int i = 0; i < imatam; i++)
        {
            unsigned char r = srcIma[i * channels + 0];
            imaRed[i * channels + 0] = r;
            imaRed[i * channels + 1] = 0;
            imaRed[i * channels + 2] = 0;
            histo[srcIma[i * channels + 0]]++;
        }
    }

    // PASO 3
    // Obtencion del cdf
    int cdf[256];
    // Primero se llena de ceros
    for (int i = 0; i < 256; i++)
    {
        cdf[i] = 0;
    }
    // Como es acumulativo se debe iniciar desde 1
    cdf[0] = histo[0];
    for (int i = 1; i < 256; i++)
    {
        cdf[i] = histo[i] + cdf[i - 1];
    }

    // PASO 4
    // Obtencion del cdfmin
    int cdfmin;
    /*Se recorre el arreglo y cuando se
    encuentre un valor distinto de cero se termina*/
    for (int i = 0; i < 256; i++)
    {
        if (cdf[i] != 0)
        {
            cdfmin = cdf[i];
            break;
        }
    }

    // PASO 5
    // Se genera eqCdf
    int eqCdf[256];
    // Se llena de ceros
    for (int i = 0; i < 256; i++)
    {
        eqCdf[i] = 0;
    }
    // Se aplica la formula a cada valor para aplicar el cambio
    double divisor = 0, dividendo = imatam - cdfmin, division = 0;
    for (int i = 0; i < 256; i++)
    {
        divisor = cdf[i] - cdfmin;
        division = divisor / dividendo;
        eqCdf[i] = (int)(round(division * (L - 2)) + 1);
    }

    // PASO 6
    // Se genera el arreglo para la nueva imagen
    unsigned char *eqImage = malloc(imatam * channels);
    // Si es de un canal se realiza la siguiente operacion
    if (channels == 1)
    {
        for (int i = 0; i < imatam; i++)
        {
            eqImage[i] = (unsigned char)eqCdf[srcIma[i]];
        }
    }
    // Si es de 3 canales se aplica de forma diferente
    if (channels == 3)
    {
        for (int i = 0; i < imatam; i++)
        {
            eqImage[i] = (unsigned char)eqCdf[srcIma[i * channels + 0]];
        }
    }

    // PASO 7
    // Se obtiene el nuevo histograma
    int eqHisto[256];
    for (int i = 0; i < 256; i++)
    {
        eqHisto[i] = 0;
    }
    // Al igual que el paso 2 se obtiene el histograma de la imagen
    for (int i = 0; i < imatam; i++)
    {
        eqHisto[eqImage[i]]++;
    }
    tiempof = omp_get_wtime();

    // PASO 8
    // Se crea la nueva imagen
    /*Primero se realizan modificaciones a los nombres
    de esta forma cada imagen conserva su nombre*/
    strncpy(nombreimagen, argv[1], strlen(argv[1]) - 4);
    strcat(nombreimagen, sufijoima);
    // Se crea la imagen y se toman los tiempos
    double tiempogeneracioni = omp_get_wtime();
    stbi_write_jpg(nombreimagen, width, height, 1, eqImage, 100);
    double tiempogeneracionf = omp_get_wtime();
    stbi_image_free(eqImage);
    double tiempogeneracionimagen = tiempogeneracionf - tiempogeneracioni;
    tiempos[1] = tiempogeneracionimagen;

    // PASO 9
    // Se crea el archivo csv
    // Tambien se modifican los nombres para el archivo
    strncpy(nombrecsv, argv[1], strlen(argv[1]) - 4);
    strcat(nombrecsv, sufijocsv);
    double tiempocsvi = omp_get_wtime();
    // Se crea el archivo y con un bucle se escriben los datos
    FILE *fp = fopen(nombrecsv, "w+");
    fprintf(fp, "%s,%s,%s\n", "Valor", "Histo", "EqHisto");
    for (int i = 0; i < 256; i++)
    {
        fprintf(fp, "%d, %d, %d\n", i, histo[i], eqHisto[i]);
    }
    fclose(fp);
    double tiempocsvf = omp_get_wtime();
    double tiempocsv = tiempocsvf - tiempocsvi;
    tiempos[2] = tiempocsv;
    // Se obtiene el tiempo secuencial y se imprime
    tiemposec = tiempof - tiempoi;
    printf("\nTiempo en secuencial: %f\n", tiemposec);
}

// Recibe como parametro lo mandado por consola
void HistogramaParalelo(char *argv[])
{
    // PASO 1
    // Se obtiene el nombre de la imagen con su ruta
    char *srcPath = argv[1];
    int width, height, channels, i;
    // Se carga la imagen y se toman los tiempos
    double tiempocargai = omp_get_wtime();
    unsigned char *srcIma = stbi_load(srcPath, &width, &height, &channels, 0);
    double tiempocargaf = omp_get_wtime();
    double tiempocargaimagen = tiempocargaf - tiempocargai;
    tiempos[3] = tiempocargaimagen;
    // Se crean las variables necesarias para los nombres de los archivos generados
    char *sufijoima2 = "_eq_parallel.jpg";
    char *sufijocsv2 = "_histo_parallel.csv";
    char nombreimagen2[50] = "";
    char nombrecsv2[50] = "";
    double tiempoi, tiempof;
    // Si la imegen no cargo se muestra un mensaje y termina la funcion
    if (srcIma == NULL)
    {
        printf("No se pudo cargar la imagen %s :(\n\n\n", srcPath);
        return;
    }
    // Si la imagen carga se muestran sus datos
    else
    {
        printf("\nImagen cargada correctamente: %dx%d srcIma con %d canales.\n", width, height, channels);
    }
    // Se obtiene el tiempo inicial
    tiempoi = omp_get_wtime();
    // Se declaran las variables necesarias en el codigo
    int cdf[256], histo[256], imatam = width * height, eqHisto[256];
    int eqCdf[256], cdfmin;
    unsigned char *eqImage = malloc(imatam * channels);
    unsigned char *imaRed = malloc(imatam * channels);
    // Se empieza a paralelizar el codigo
#pragma omp parallel
    {
        // PASO 2
        // Mediante un bucle se llenan de ceros los arreglos necesarios
#pragma omp for
        for (i = 0; i < 256; i++)
        {
            histo[i] = 0;
            cdf[i] = 0;
            eqCdf[i] = 0;
            eqHisto[i] = 0;
        }
        // Se debe de esperar a que todos terminen
#pragma omp barrier

        // Si es de 1 canal se usa reduction y se obtiene el histograma
        if (channels == 1)
        {
#pragma omp for reduction(+ \
                          : histo)
            for (int i = 0; i < imatam; i++)
            {
                histo[srcIma[i]]++;
            }
        }
        /*Si es de 3 canales se usa reduction y se obtiene el histograma
        de una forma diferente*/
        if (channels == 3)
        {
#pragma omp for reduction(+ \
                          : histo) private(i)
            for (int i = 0; i < imatam; i++)
            {
                unsigned char r = srcIma[i * channels + 0];
                imaRed[i * channels + 0] = r;
                imaRed[i * channels + 1] = 0;
                imaRed[i * channels + 2] = 0;
                histo[srcIma[i * channels + 0]]++;
            }
        }

        // PASO 3
        /*Se obtiene el cdf esto no se debe de paralelizar
        por ello se usa single*/
#pragma omp single
        {
            cdf[0] = histo[0];
            {
                for (i = 1; i < 256; i++)
                {
                    cdf[i] = histo[i] + cdf[i - 1];
                }
            }
        }

// PASO 4
/*Se obtiene el cdfmin, al buscar el primer valor distinto a cero
no se paralelizo*/
#pragma omp single
        {
            for (i = 0; i < 256; i++)
            {
                if (cdf[i] != 0)
                {
                    cdfmin = cdf[i];
                    break;
                }
            }
        }

        // PASO 5
        // Mediante un bucle for se obtienen los valores con una i privada
#pragma omp for private(i)
        for (i = 0; i < 256; i++)
        {
            double divisor = 0, dividendo = imatam - cdfmin, division = 0;
            divisor = cdf[i] - cdfmin;
            division = divisor / dividendo;
            eqCdf[i] = (int)(round(division * (L - 2)) + 1);
        }

        // PASO 6
        // Si es de 1 canal se obtiene eqImage de cierta forma
        if (channels == 1)
        {
#pragma omp for private(i)
            for (int i = 0; i < imatam; i++)
            {
                eqImage[i] = (unsigned char)eqCdf[srcIma[i]];
            }
        }
        // Si es de 3 canales se obtiene eqImage de cierta forma por los canales
        if (channels == 3)
        {
#pragma omp for private(i)
            for (int i = 0; i < imatam; i++)
            {
                eqImage[i] = (unsigned char)eqCdf[srcIma[i * channels + 0]];
            }
        }

        // PASO 7
        /*Para obtener el histograma de este nuevo arreglo se
        uso reduction*/
#pragma omp for reduction(+ \
                          : eqHisto)
        for (i = 0; i < imatam; i++)
        {
            eqHisto[eqImage[i]]++;
        }
        tiempof = omp_get_wtime();

// Dado que los siguientes pasos no se pueden paralelizar se usa single
#pragma omp single
        {
            // PASO 8
            // Se modifican arreglos para darle nombre a los archivos
            strncpy(nombreimagen2, argv[1], strlen(argv[1]) - 4);
            strcat(nombreimagen2, sufijoima2);
            // Se crea la imagen con el nuevo histograma
            double tiempogeneracioni = omp_get_wtime();
            stbi_write_jpg(nombreimagen2, width, height, 1, eqImage, 100);
            double tiempogeneracionf = omp_get_wtime();
            stbi_image_free(eqImage);
            double tiempogeneracionimagen = tiempogeneracionf - tiempogeneracioni;
            tiempos[4] = tiempogeneracionimagen;

            // PASO 9
            // Se modifican arreglos para darle nombre a los archivos
            strncpy(nombrecsv2, argv[1], strlen(argv[1]) - 4);
            strcat(nombrecsv2, sufijocsv2);
            double tiempocsvi = omp_get_wtime();
            // Se crea el archivo y se va escribiendo
            FILE *fp = fopen(nombrecsv2, "w+");
            fprintf(fp, "%s,%s,%s\n", "Valor", "Histo", "EqHisto");
            for (i = 0; i < 256; i++)
            {
                fprintf(fp, "%d, %d, %d\n", i, histo[i], eqHisto[i]);
            }
            fclose(fp);
            double tiempocsvf = omp_get_wtime();
            double tiempocsv = tiempocsvf - tiempocsvi;
            tiempos[5] = tiempocsv;
        }
    }
    // Se obtiene el tiempo final y se imprime
    tiempopar = tiempof - tiempoi;
    printf("\nTiempo en paralelo: %f\n", tiempopar);
}