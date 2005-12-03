
/* $Id: //sw/main/apps/OpenGL/mjk/shadowcast/bluepony.c#4 $ */

#include <assert.h>
#include <stdlib.h>
#include <GL/glut.h>

extern void setAmbientAndDiffuseMaterial(GLfloat *material);
extern int useDisplayLists;

/************************ Pony-specific code *********************************/

static float BodyDepth = 0.216;

static GLfloat BodyVerts[][2] =
{
  {-0.993334, 0.344444},
  {-0.72, 0.462964},
  {-0.58, -0.411113},
  {-0.406667, -0.692593},
  {0.733334, -0.633334},
  {0.846666, -0.225926},
  {0.873335, -0.55926},
  {0.879998, -0.988888},
  {0.933332, -0.974074},
  {0.953334, -0.537037},
  {0.906667, -0.0777776},
  {0.806666, 0.0333334},
  {-0.26, 0.0111111},
  {-0.406667, 0.27037},
  {-0.54, 0.781481},
  {-0.673333, 1.00371},
  {-0.653332, 0.803704},
  {-1.05333, 0.44815}
};

static float LegDepth = 0.144;

static float FrontLegPos[3] =
{-0.36, -0.324, 0.108};
static GLfloat FrontLegVerts[][2] =
{
  {-0.23, -0.113481},
  {-0.123333, -0.528296},
  {-0.0926752, -0.728103},
  {-0.0766667, -1.232},
  {0.0233333, -1.232},
  {0.0433332, -0.743111},
  {0.0366667, -0.424593},
  {0.0699998, -0.157926},
  {0.116667, 0.049482},
  {-0.0166667, 0.197629},
  {-0.196667, 0.13837}
};

static float BackLegPos[3] =
{0.684, -0.324, 0.108};
static GLfloat BackLegVerts[][2] =
{
  {-0.24, -0.195556},
  {-0.0933332, -0.41037},
  {-0.04, -0.684445},
  {-0.113333, -1.26222},
  {0, -1.26222},
  {0.1, -0.677037},
  {0.213333, -0.121482},
  {0.153333, 0.108148},
  {-0.0533333, 0.211853},
  {-0.26, 0.063702}
};

static float ManeDepth = 0.288;
static GLfloat ManeVerts[][2] =
{
  {-0.512667, 0.578519},
  {-0.419333, 0.267407},
  {-0.299333, -0.00666719},
  {-0.239333, -0.0140724},
  {-0.226, 0.0896296},
  {-0.319333, 0.422963},
  {-0.532667, 0.741481}
};

static float EyePos[3] =
{-0.702, 0.648, 0.1116};
static float EyeSize = 0.025;

/* Display lists */
static GLuint Body = 0, FrontLeg = 0, BackLeg = 0, Mane = 0;

/* Generate an extruded, capped part from a 2-D polyline. */
static void 
ExtrudePart(int n, GLfloat v[][2], float depth)
{
  static GLUtriangulatorObj *tobj = NULL;
  int i;
  float z0 = 0.5 * depth;
  float z1 = -0.5 * depth;
  GLdouble vertex[3];

  if (tobj == NULL) {
    tobj = gluNewTess();  /* create and initialize a GLU polygon * *
                             tesselation object */
    gluTessCallback(tobj, GLU_BEGIN, (void (__stdcall *)(void))glBegin);
    gluTessCallback(tobj, GLU_VERTEX, (void (__stdcall *)(void))glVertex2fv);  /* semi-tricky */
    gluTessCallback(tobj, GLU_END, glEnd);
  }
  /* +Z face */
  glPushMatrix();
  glTranslatef(0.0, 0.0, z0);
  glNormal3f(0.0, 0.0, 1.0);
  gluBeginPolygon(tobj);
  for (i = 0; i < n; i++) {
    vertex[0] = v[i][0];
    vertex[1] = v[i][1];
    vertex[2] = 0.0;
    gluTessVertex(tobj, vertex, v[i]);
  }
  gluEndPolygon(tobj);
  glPopMatrix();

  /* -Z face */
  glFrontFace(GL_CW);
  glPushMatrix();
  glTranslatef(0.0, 0.0, z1);
  glNormal3f(0.0, 0.0, -1.0);
  gluBeginPolygon(tobj);
  for (i = 0; i < n; i++) {
    vertex[0] = v[i][0];
    vertex[1] = v[i][1];
    vertex[2] = z1;
    gluTessVertex(tobj, vertex, v[i]);
  }
  gluEndPolygon(tobj);
  glPopMatrix();

  glFrontFace(GL_CCW);
  /* edge polygons */
  glBegin(GL_TRIANGLE_STRIP);
  for (i = 0; i <= n; i++) {
    float x = v[i % n][0];
    float y = v[i % n][1];
    float dx = v[(i + 1) % n][0] - x;
    float dy = v[(i + 1) % n][1] - y;
    glVertex3f(x, y, z0);
    glVertex3f(x, y, z1);
    glNormal3f(dy, -dx, 0.0);
  }
  glEnd();

}

static GLfloat blue[4] =
{0.1, 0.1, 1.0, 1.0};
static GLfloat black[4] =
{0.0, 0.0, 0.0, 1.0};
static GLfloat pink[4] =
{1.0, 0.5, 0.5, 1.0};

static void
DoBody(void)
{
  setAmbientAndDiffuseMaterial(blue);
  ExtrudePart(sizeof(BodyVerts) / sizeof(GLfloat) / 2, BodyVerts, BodyDepth);

  /* eyes */
  setAmbientAndDiffuseMaterial(black);
  glNormal3f(0.0, 0.0, 1.0);
  glBegin(GL_POLYGON);
  glVertex3f(EyePos[0] - EyeSize, EyePos[1] - EyeSize, EyePos[2]);
  glVertex3f(EyePos[0] + EyeSize, EyePos[1] - EyeSize, EyePos[2]);
  glVertex3f(EyePos[0] + EyeSize, EyePos[1] + EyeSize, EyePos[2]);
  glVertex3f(EyePos[0] - EyeSize, EyePos[1] + EyeSize, EyePos[2]);
  glEnd();
  glNormal3f(0.0, 0.0, -1.0);
  glBegin(GL_POLYGON);
  glVertex3f(EyePos[0] - EyeSize, EyePos[1] + EyeSize, -EyePos[2]);
  glVertex3f(EyePos[0] + EyeSize, EyePos[1] + EyeSize, -EyePos[2]);
  glVertex3f(EyePos[0] + EyeSize, EyePos[1] - EyeSize, -EyePos[2]);
  glVertex3f(EyePos[0] - EyeSize, EyePos[1] - EyeSize, -EyePos[2]);
  glEnd();
}

static void
DoMane(void)
{
  setAmbientAndDiffuseMaterial(pink);
  ExtrudePart(sizeof(ManeVerts) / sizeof(GLfloat) / 2, ManeVerts, ManeDepth);
}

static void
DoFrontLeg(void)
{
  setAmbientAndDiffuseMaterial(blue);
  ExtrudePart(sizeof(FrontLegVerts) / sizeof(GLfloat) / 2,
    FrontLegVerts, LegDepth);
}

static void
DoBackLeg(void)
{
  setAmbientAndDiffuseMaterial(blue);
  ExtrudePart(sizeof(BackLegVerts) / sizeof(GLfloat) / 2,
    BackLegVerts, LegDepth);
}

/* 
 * Build the four display lists which make up the pony.
 */
static void 
MakePony(void)
{
  Body = glGenLists(1);
  glNewList(Body, GL_COMPILE);
  DoBody();
  glEndList();

  Mane = glGenLists(1);
  glNewList(Mane, GL_COMPILE);
  DoMane();
  glEndList();

  FrontLeg = glGenLists(1);
  glNewList(FrontLeg, GL_COMPILE);
  DoFrontLeg();
  glEndList();

  BackLeg = glGenLists(1);
  glNewList(BackLeg, GL_COMPILE);
  DoBackLeg();
  glEndList();
}

/* 
 * Draw the pony.  legAngle should be in [-15,15] or so.
 * The pony display lists will be constructed the first time this is called.
 */
void 
DrawPony(float legAngle)
{
  if (!useDisplayLists) {
    /* BODY */
    DoBody();

    /* MANE */
    DoMane();

    /* FRONT +Z LEG */
    glPushMatrix();
    glTranslatef(FrontLegPos[0], FrontLegPos[1], FrontLegPos[2]);
    glRotatef(legAngle, 0.0, 0.0, 1.0);
    DoFrontLeg();
    glPopMatrix();

    /* FRONT -Z LEG */
    glPushMatrix();
    glTranslatef(FrontLegPos[0], FrontLegPos[1], -FrontLegPos[2]);
    glRotatef(-legAngle, 0.0, 0.0, 1.0);
    DoFrontLeg();
    glPopMatrix();

    /* BACK +Z LEG */
    glPushMatrix();
    glTranslatef(BackLegPos[0], BackLegPos[1], BackLegPos[2]);
    glRotatef(-legAngle, 0.0, 0.0, 1.0);
    DoBackLeg();
    glPopMatrix();

    /* BACK -Z LEG */
    glPushMatrix();
    glTranslatef(BackLegPos[0], BackLegPos[1], -BackLegPos[2]);
    glRotatef(legAngle, 0.0, 0.0, 1.0);
    DoBackLeg();
    glPopMatrix();

    return;
  }

  if (!Body) {
    MakePony();
  }
  assert(Body);

  /* BODY */
  glCallList(Body);

  /* MANE */
  glCallList(Mane);

  /* FRONT +Z LEG */
  glPushMatrix();
  glTranslatef(FrontLegPos[0], FrontLegPos[1], FrontLegPos[2]);
  glRotatef(legAngle, 0.0, 0.0, 1.0);
  glCallList(FrontLeg);
  glPopMatrix();

  /* FRONT -Z LEG */
  glPushMatrix();
  glTranslatef(FrontLegPos[0], FrontLegPos[1], -FrontLegPos[2]);
  glRotatef(-legAngle, 0.0, 0.0, 1.0);
  glCallList(FrontLeg);
  glPopMatrix();

  /* BACK +Z LEG */
  glPushMatrix();
  glTranslatef(BackLegPos[0], BackLegPos[1], BackLegPos[2]);
  glRotatef(-legAngle, 0.0, 0.0, 1.0);
  glCallList(BackLeg);
  glPopMatrix();

  /* BACK -Z LEG */
  glPushMatrix();
  glTranslatef(BackLegPos[0], BackLegPos[1], -BackLegPos[2]);
  glRotatef(legAngle, 0.0, 0.0, 1.0);
  glCallList(BackLeg);
  glPopMatrix();
}

/************************* end of pony code **********************************/

