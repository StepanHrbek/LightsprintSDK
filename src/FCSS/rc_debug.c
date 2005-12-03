
/* rc_debug.c - print current register combiners state for debugging */

/* Copyright NVIDIA Corporation, 2000. */

/* $Id: //sw/main/apps/OpenGL/mjk/shadowcast/rc_debug.c#3 $ */

#include <stdio.h>
#include <GL/glut.h>
#include <GL/glprocs.h>

static char *varStr[] = {
    "A", "B", "C", "D", "E", "F", "G"
};
static char *portionStr[] = {
    "rgb  ", "alpha"
};
static char *componentStr[] = {
    "alpha", "rgb"
};
static char *mappingStr[] = {
    "unsignedIdentity",
    "unsignedInvert",
    "expandNormal",
    "expandNegate",
    "halfBiasNormal",
    "halfBiasNegate",
    "signedIdentity",
    "signedInvert"
};

static char *
registerStr(GLenum input)
{
    switch (input) {
    case GL_DISCARD_NV:
        return "DISCARDED";
    case GL_ZERO:
        return "zero";
    case GL_CONSTANT_COLOR0_NV:
        return "constantColor0";
    case GL_CONSTANT_COLOR1_NV:
        return "constantColor1";
    case GL_FOG:
        return "fog";
    case GL_PRIMARY_COLOR_NV:
        return "primaryColor";
    case GL_SECONDARY_COLOR_NV:
        return "secondaryColor";
    case GL_SPARE0_NV:
        return "spare0";
    case GL_SPARE1_NV:
        return "spare1";
    case GL_TEXTURE0_ARB:
        return "texture0";
    case GL_TEXTURE1_ARB:
        return "texture1";
    case GL_E_TIMES_F_NV:
        return "EtimesF";
    case GL_SPARE0_PLUS_SECONDARY_COLOR_NV:
        return "spare0+secondaryColor";
    default:
        return "unknown";
    }
}

static char *
biasStr(GLenum bias)
{
    switch (bias) {
    case GL_NONE:
        return "";
    case GL_BIAS_BY_NEGATIVE_ONE_HALF_NV:
        return " - 0.5";
    default:
        return " + unknown";
    }
}

static char *
scaleStr(GLenum scale)
{
    switch (scale) {
    case GL_NONE:
        return "";
    case GL_SCALE_BY_TWO_NV:
        return " * 2";
    case GL_SCALE_BY_FOUR_NV:
        return " * 4";
    case GL_SCALE_BY_ONE_HALF_NV:
        return " * 0.5";
    default:
        return " * unknown";
    }
}

void
printRegisterCombinersState(void)
{
  GLint numCombiners;
  GLenum input, mapping, component;
  GLboolean clampSum;
  GLint abDot, cdDot, muxSum, scale, bias, abOutput, cdOutput, sumOutput;
  GLfloat color[4];
  int stage, portion, var;

  printf("--------------\n");
  if (glIsEnabled(GL_REGISTER_COMBINERS_NV)) {
    printf("  register combiners ENABLED\n");
    glGetFloatv(GL_CONSTANT_COLOR0_NV, color);
    printf("constantColor0 = ( %f, %f, %f, %f )\n", color[0], color[1], color[2], color[3]);
    glGetFloatv(GL_CONSTANT_COLOR1_NV, color);
    printf("constantColor1 = ( %f, %f, %f, %f )\n", color[0], color[1], color[2], color[3]);
    glGetIntegerv(GL_NUM_GENERAL_COMBINERS_NV, &numCombiners);
    for (stage=0; stage<numCombiners; stage++) {
        for (portion=0; portion<2; portion++) {
            printf("%d.%s\n", stage, portionStr[portion]);
            for (var=0; var<4; var++) {
                glGetCombinerInputParameterivNV(GL_COMBINER0_NV+stage,
                    portion ? GL_ALPHA : GL_RGB,
                    GL_VARIABLE_A_NV+var,
                    GL_COMBINER_INPUT_NV, &input);
                glGetCombinerInputParameterivNV(GL_COMBINER0_NV+stage,
                    portion ? GL_ALPHA : GL_RGB,
                    GL_VARIABLE_A_NV+var, GL_COMBINER_MAPPING_NV, &mapping);
                glGetCombinerInputParameterivNV(GL_COMBINER0_NV+stage,
                    portion ? GL_ALPHA : GL_RGB,
                    GL_VARIABLE_A_NV+var, GL_COMBINER_COMPONENT_USAGE_NV, &component);
                printf("    %s = %s(%s).%s\n",
                    varStr[var],
                    mappingStr[mapping-GL_UNSIGNED_IDENTITY_NV],
                    registerStr(input), componentStr[component == GL_RGB]);
            }
            /* WARNING: Bugs in the Release 4 NVIDIA drivers mean that
               the GL_COMBINER_AB_DOT_PRODUCT_NV,
               GL_COMBINER_CD_DOT_PRODUCT_NV, GL_COMBINER_AB_OUTPUT_NV,
               GL_COMBINER_CD_OUTPUT_NV, and GL_COMBINER_SUM_OUTPUT_NV
               gets for glGetCombinerOutputParameterivNV and
               glGetCombinerOutputParameterfvNV return in correct values.
               This bug is fixed in Release 4 and later drivers. */
            glGetCombinerOutputParameterivNV(GL_COMBINER0_NV+stage,
                portion ? GL_ALPHA : GL_RGB,
                GL_COMBINER_AB_DOT_PRODUCT_NV, &abDot);
            glGetCombinerOutputParameterivNV(GL_COMBINER0_NV+stage,
                portion ? GL_ALPHA : GL_RGB,
                GL_COMBINER_CD_DOT_PRODUCT_NV, &cdDot);
            glGetCombinerOutputParameterivNV(GL_COMBINER0_NV+stage,
                portion ? GL_ALPHA : GL_RGB,
                GL_COMBINER_MUX_SUM_NV, &muxSum);
            glGetCombinerOutputParameterivNV(GL_COMBINER0_NV+stage,
                portion ? GL_ALPHA : GL_RGB,
                GL_COMBINER_SCALE_NV, &scale);
            glGetCombinerOutputParameterivNV(GL_COMBINER0_NV+stage,
                portion ? GL_ALPHA : GL_RGB,
                GL_COMBINER_BIAS_NV, &bias);
            glGetCombinerOutputParameterivNV(GL_COMBINER0_NV+stage,
                portion ? GL_ALPHA : GL_RGB,
                GL_COMBINER_AB_OUTPUT_NV, &abOutput);
            glGetCombinerOutputParameterivNV(GL_COMBINER0_NV+stage,
                portion ? GL_ALPHA : GL_RGB,
                GL_COMBINER_CD_OUTPUT_NV, &cdOutput);
            glGetCombinerOutputParameterivNV(GL_COMBINER0_NV+stage,
                portion ? GL_ALPHA : GL_RGB,
                GL_COMBINER_SUM_OUTPUT_NV, &sumOutput);
            printf("    ----\n");
            printf("    %s = %sA %s B%s%s%s\n",
                registerStr(abOutput),
                (scale != GL_NONE) ? "(" : "",
                abDot ? "dot" : "times",
                biasStr(bias),
                (scale != GL_NONE) ? ")" : "",
                scaleStr(scale));
            printf("    %s = %sC %s D%s%s%s\n",
                registerStr(cdOutput),
                (scale != GL_NONE) ? "(" : "",
                cdDot ? "dot" : "times",
                biasStr(bias),
                (scale != GL_NONE) ? ")" : "",
                scaleStr(scale));
            printf("    %s = %s(A times B) %s (C times D)%s%s%s\n",
                registerStr(sumOutput),
                (scale != GL_NONE) ? "(" : "",
                muxSum ? "mux" : "+",
                biasStr(bias),
                (scale != GL_NONE) ? ")" : "",
                scaleStr(scale));
        }
    }
    printf("final combiner\n", stage, portionStr[portion]);
    glGetBooleanv(GL_COLOR_SUM_CLAMP_NV, &clampSum);
    printf("  color clamp sum %s\n", clampSum ? "ENABLED" : "DISABLED");
    for (var=0; var<7; var++) {
        glGetFinalCombinerInputParameterivNV(GL_VARIABLE_A_NV+var,
            GL_COMBINER_INPUT_NV, &input);
        glGetFinalCombinerInputParameterivNV(GL_VARIABLE_A_NV+var,
            GL_COMBINER_MAPPING_NV, &mapping);
        glGetFinalCombinerInputParameterivNV(GL_VARIABLE_A_NV+var,
            GL_COMBINER_COMPONENT_USAGE_NV, &component);
        printf("    %s = %s(%s).%s\n",
            varStr[var],
            mappingStr[mapping-GL_UNSIGNED_IDENTITY_NV],
            registerStr(input), componentStr[component == GL_RGB]);
    }
    printf("    ----\n");
    printf("    RGB   = A times B plus (1-A) times C plus D\n");
    printf("    Alpha = G\n");
  } else {
    printf("  register combiners DISABLED\n");
  }
  printf("--------------\n");
}
