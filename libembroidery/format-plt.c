#include <stdio.h>
#include <string.h>
#include "format-pec.h"
#include "format-phc.h"
#include "helpers-binary.h"
#include "helpers-misc.h"

int readPlt(EmbPattern* pattern, const char* fileName)
{
    double x, y;
    double scalingFactor = 40;
    char input[512];
    FILE* file = 0;

    file = fopen(fileName, "rb");
    if(!file)
    {
        return 0;
    }
    embPattern_loadExternalColorFile(pattern, fileName);
    while(fscanf(file, "%s", input) >= 0)
    {
        if(startsWith("PD", input))
        {
            if(sscanf(input, "PD%lf,%lf;", &x, &y) < 2)
            {
                break;
            }
            embPattern_addStitchAbs(pattern, x / scalingFactor, y / scalingFactor, NORMAL, 1);
        }
        else if(startsWith("PU", input))
        {
            if(sscanf(input, "PU%lf,%lf;", &x, &y) < 2)
            {
                break;
            }
            embPattern_addStitchAbs(pattern, x / scalingFactor, y / scalingFactor, STOP, 1);
        }
    }
    embPattern_addStitchRel(pattern, 0.0, 0.0, END, 1);
    fclose(file);
    return 1;
}

int writePlt(EmbPattern* pattern, const char* fileName)
{
    /* TODO: pointer safety */
    double scalingFactor = 40;
    EmbStitch stitch;
    EmbStitchList* pointer = 0;
    char firstStitchOfBlock = 1;
    FILE* file = 0;

    file = fopen(fileName, "wb");
    if(!file)
    {
        return 0;
    }
    fprintf(file, "IN;");
    fprintf(file, "ND;");

    pointer = pattern->stitchList;
    while(pointer)
    {
        stitch = pointer->stitch;
        if(stitch.flags & STOP)
        {
            firstStitchOfBlock = 1;
        }
        if(firstStitchOfBlock)
        {
            fprintf(file, "PU%lf,%lf;", stitch.xx * scalingFactor, stitch.yy * scalingFactor);
            fprintf(file, "ST0.00,0.00;");
            fprintf(file, "SP0;");
            fprintf(file, "HT0;");
            fprintf(file, "HS0;");
            fprintf(file, "TT0;");
            fprintf(file, "TS0;");
            firstStitchOfBlock = 0;
        }
        else
        {
            fprintf(file, "PD%lf,%lf;", stitch.xx * scalingFactor, stitch.yy * scalingFactor);
        }

        pointer = pointer->next;
    }
    fprintf(file, "PU0.0,0.0;");
    fprintf(file, "PU0.0,0.0;");
    fclose(file);
    return 1; /*TODO: finish WritePlt */
}

/* kate: bom off; indent-mode cstyle; indent-width 4; replace-trailing-space-save on; */

