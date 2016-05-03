#include "urFont.h"
//--------------------------

#include "config.h"
#ifndef UISTRINGS
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>

// from ofUtils.cpp
#include <algorithm>

#if defined(TARGET_IPHONE) || defined(TARGET_OSX ) || defined(TARGET_LINUX)
#include "sys/time.h"
#endif

#ifdef TARGET_WIN32
#include <mmsystem.h>
#ifdef _MSC_VER
#include <direct.h>
#endif

#endif

#ifdef OPENGLES2
enum { ATTRIB_VERTEX, ATTRIB_COLOR, ATTRIB_TEXTUREPOSITON, NUM_ATTRIBUTES };
#endif

void urLoadIdentity();
void urPopMatrix();
void urTranslatef(GLfloat x, GLfloat y, GLfloat z);
void urRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void urPushMatrix();

static bool enableDataPath = true;

//use ofSetDataPathRoot() to override this
#if defined TARGET_OSX
static string dataPathRoot = "../../../data/";
#else
static string dataPathRoot = "./";
#endif

//--------------------------------------------------
void ofSetDataPathRoot(string newRoot){
	dataPathRoot = newRoot;
}

//--------------------------------------------------
string ofToDataPath(string path, bool makeAbsolute){
	if( enableDataPath ){

		//check if absolute path has been passed or if data path has already been applied
		//do we want to check for C: D: etc ?? like  substr(1, 2) == ':' ??
		if( path.substr(0,1) != "/" &&  path.substr(1,1) != ":" &&  path.substr(0,dataPathRoot.length()) != dataPathRoot){
			path = dataPathRoot+path;
		}

		if(makeAbsolute && path.substr(0,1) != "/"){
#if 0 // ignore makeAbsolute
#ifndef TARGET_IPHONE
#ifndef _MSC_VER
			char currDir[1024];
			path = "/"+path;
			path = getcwd(currDir, 1024)+path;
#else
			char currDir[1024];
			path = "\\"+path;
			path = _getcwd(currDir, 1024)+path;
			std::replace( path.begin(), path.end(), '/', '\\' ); // fix any unixy paths...
#endif
#else
			//do we need iphone specific code here?
#endif
#endif
		}

	}
	return path;
}

// from urFont.cpp
static bool printVectorInfo = false;

typedef struct{
	urPoint P0;
	urPoint P1;
}Segment;

// dot product (3D) which allows vector operations in arguments
#define dot(u,v)   ((u).x * (v).x + (u).y * (v).y + (u).z * (v).z)
#define norm2(v)   dot(v,v)        // norm2 = squared length of vector
#define norm(v)    sqrt(norm2(v))  // norm = length of vector
#define d2(u,v)    norm2(u-v)      // distance squared = norm2 of difference
#define d(u,v)     norm(u-v)       // distance = norm of difference

static void simplifyDP(float tol, urPoint* v, int j, int k, int* mk ){
	if (k <= j+1) // there is nothing to simplify
		return;

	// check for adequate approximation by segment S from v[j] to v[k]
	int     maxi	= j;          // index of vertex farthest from S
	float   maxd2	= 0;         // distance squared of farthest vertex
	float   tol2	= tol * tol;  // tolerance squared
	Segment S		= {v[j], v[k]};  // segment from v[j] to v[k]
	urPoint u;
	u				= S.P1 - S.P0;   // segment direction vector
	double  cu		= dot(u,u);     // segment length squared

	urPoint  w;
	urPoint   Pb;                // base of perpendicular from v[i] to S
	float  b, cw, dv2;        // dv2 = distance v[i] to S squared

	for (int i=j+1; i<k; i++){
		// compute distance squared
		w = v[i] - S.P0;
		cw = dot(w,u);
		if ( cw <= 0 ) dv2 = d2(v[i], S.P0);
		else if ( cu <= cw ) dv2 = d2(v[i], S.P1);
		else {
			b = (float)(cw / cu);
			Pb = S.P0 + u*b;
			dv2 = d2(v[i], Pb);
		}
		// test with current max distance squared
		if (dv2 <= maxd2) continue;

		// v[i] is a new max vertex
		maxi = i;
		maxd2 = dv2;
	}
	if (maxd2 > tol2)        // error is worse than the tolerance
	{
		// split the polyline at the farthest vertex from S
		mk[maxi] = 1;      // mark v[maxi] for the simplified polyline
		// recursively simplify the two subpolylines at v[maxi]
		simplifyDP( tol, v, j, maxi, mk );  // polyline v[j] to v[maxi]
		simplifyDP( tol, v, maxi, k, mk );  // polyline v[maxi] to v[k]
	}
	// else the approximation is OK, so ignore intermediate vertices
	return;
}


//-------------------------------------------------------------------
// needs simplifyDP which is above
static vector <urPoint> ofSimplifyContour(vector <urPoint> &V, float tol){
	int n = V.size();

	vector <urPoint> sV;
	sV.assign(n, urPoint());

	int    i, k, m, pv;            // misc counters
	float  tol2 = tol * tol;       // tolerance squared
	urPoint * vt = new urPoint[n];
	int * mk = new int[n];

	memset(mk, 0, sizeof(int) * n );

	// STAGE 1.  Vertex Reduction within tolerance of prior vertex cluster
	vt[0] = V[0];              // start at the beginning
	for (i=k=1, pv=0; i<n; i++) {
		if (d2(V[i], V[pv]) < tol2) continue;

		vt[k++] = V[i];
		pv = i;
	}
	if (pv < n-1) vt[k++] = V[n-1];      // finish at the end

	// STAGE 2.  Douglas-Peucker polyline simplification
	mk[0] = mk[k-1] = 1;       // mark the first and last vertices
	simplifyDP( tol, vt, 0, k-1, mk );

	// copy marked vertices to the output simplified polyline
	for (i=m=0; i<k; i++) {
		if (mk[i]) sV[m++] = vt[i];
	}

	//get rid of the unused points
	if( m < (int)sV.size() ) sV.erase( sV.begin()+m, sV.end() );

	delete [] vt;
	delete [] mk;

	return sV;
}


//------------------------------------------------------------
static void quad_bezier(vector <urPoint> &ptsList, float x1, float y1, float x2, float y2, float x3, float y3, int res){
	for(int i=0; i <= res; i++){
		double t = (double)i / (double)(res);
		double a = pow((1.0 - t), 2.0);
		double b = 2.0 * t * (1.0 - t);
		double c = pow(t, 2.0);
		double x = a * x1 + b * x2 + c * x3;
		double y = a * y1 + b * y2 + c * y3;
		ptsList.push_back(urPoint((float)x, (float)y));
	}
}

//-----------------------------------------------------------
static void cubic_bezier(vector <urPoint> &ptsList, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, int res){
	float   ax, bx, cx;
	float   ay, by, cy;
	float   t, t2, t3;
	float   x, y;

	// polynomial coefficients
	cx = 3.0f * (x1 - x0);
	bx = 3.0f * (x2 - x1) - cx;
	ax = x3 - x0 - cx - bx;

	cy = 3.0f * (y1 - y0);
	by = 3.0f * (y2 - y1) - cy;
	ay = y3 - y0 - cy - by;


	int resolution = res;

	for (int i = 0; i < resolution; i++){
		t 	=  (float)i / (float)(resolution-1);
		t2 = t * t;
		t3 = t2 * t;
		x = (ax * t3) + (bx * t2) + (cx * t) + x0;
		y = (ay * t3) + (by * t2) + (cy * t) + y0;
		ptsList.push_back(urPoint(x,y) );
	}
}

//--------------------------------------------------------
static ofTTFCharacter makeContoursForCharacter(FT_Face &face);
static ofTTFCharacter makeContoursForCharacter(FT_Face &face){

	//int num			= face->glyph->outline.n_points;
	int nContours	= face->glyph->outline.n_contours;
	int startPos	= 0;

	char * tags		= face->glyph->outline.tags;
	FT_Vector * vec = face->glyph->outline.points;

	ofTTFCharacter charOutlines;

	for(int k = 0; k < nContours; k++){
		if( k > 0 ){
			startPos = face->glyph->outline.contours[k-1]+1;
		}
		int endPos = face->glyph->outline.contours[k]+1;

		if( printVectorInfo )printf("--NEW CONTOUR\n\n");

		vector <urPoint> testOutline;
		urPoint lastPoint;

		for(int j = startPos; j < endPos; j++){

			if( FT_CURVE_TAG(tags[j]) == FT_CURVE_TAG_ON ){
				lastPoint.set((float)vec[j].x, (float)-vec[j].y, 0);
				if( printVectorInfo )printf("flag[%i] is set to 1 - regular point - %f %f \n", j, lastPoint.x, lastPoint.y);
				testOutline.push_back(lastPoint);

			}else{
				if( printVectorInfo )printf("flag[%i] is set to 0 - control point \n", j);

				if( FT_CURVE_TAG(tags[j]) == FT_CURVE_TAG_CUBIC ){
					if( printVectorInfo )printf("- bit 2 is set to 2 - CUBIC\n");

					int prevPoint = j-1;
					if( j == 0){
						prevPoint = endPos-1;
					}

					int nextIndex = j+1;
					if( nextIndex >= endPos){
						nextIndex = startPos;
					}

					urPoint nextPoint( (float)vec[nextIndex].x,  -(float)vec[nextIndex].y );

					//we need two control points to draw a cubic bezier
					bool lastPointCubic =  ( FT_CURVE_TAG(tags[prevPoint]) != FT_CURVE_TAG_ON ) && ( FT_CURVE_TAG(tags[prevPoint]) == FT_CURVE_TAG_CUBIC);

					if( lastPointCubic ){
						urPoint controlPoint1((float)vec[prevPoint].x,	(float)-vec[prevPoint].y);
						urPoint controlPoint2((float)vec[j].x, (float)-vec[j].y);
						urPoint nextPoint((float) vec[nextIndex].x,	-(float) vec[nextIndex].y);

						cubic_bezier(testOutline, lastPoint.x, lastPoint.y, controlPoint1.x, controlPoint1.y, controlPoint2.x, controlPoint2.y, nextPoint.x, nextPoint.y, 8);
					}

				}else{

					urPoint conicPoint( (float)vec[j].x,  -(float)vec[j].y );

					if( printVectorInfo )printf("- bit 2 is set to 0 - conic- \n");
					if( printVectorInfo )printf("--- conicPoint point is %f %f \n", conicPoint.x, conicPoint.y);

					//If the first point is connic and the last point is connic then we need to create a virutal point which acts as a wrap around
					if( j == startPos ){
						bool prevIsConnic = (  FT_CURVE_TAG( tags[endPos-1] ) != FT_CURVE_TAG_ON ) && ( FT_CURVE_TAG( tags[endPos-1]) != FT_CURVE_TAG_CUBIC );

						if( prevIsConnic ){
							urPoint lastConnic((float)vec[endPos - 1].x, (float)-vec[endPos - 1].y);
							lastPoint = (conicPoint + lastConnic) / 2;

							if( printVectorInfo )	printf("NEED TO MIX WITH LAST\n");
							if( printVectorInfo )printf("last is %f %f \n", lastPoint.x, lastPoint.y);
						}
					}

					//bool doubleConic = false;

					int nextIndex = j+1;
					if( nextIndex >= endPos){
						nextIndex = startPos;
					}

					urPoint nextPoint( (float)vec[nextIndex].x,  -(float)vec[nextIndex].y );

					if( printVectorInfo )printf("--- last point is %f %f \n", lastPoint.x, lastPoint.y);

					bool nextIsConnic = (  FT_CURVE_TAG( tags[nextIndex] ) != FT_CURVE_TAG_ON ) && ( FT_CURVE_TAG( tags[nextIndex]) != FT_CURVE_TAG_CUBIC );

					//create a 'virtual on point' if we have two connic points
					if( nextIsConnic ){
						nextPoint = (conicPoint + nextPoint) / 2;
						if( printVectorInfo )printf("|_______ double connic!\n");
					}
					if( printVectorInfo )printf("--- next point is %f %f \n", nextPoint.x, nextPoint.y);

					quad_bezier(testOutline, lastPoint.x, lastPoint.y, conicPoint.x, conicPoint.y, nextPoint.x, nextPoint.y, 8);

					if( nextIsConnic ){
						lastPoint = nextPoint;
					}
				}
			}

			//end for
		}

		for(int g =0; g < (int)testOutline.size(); g++){
			testOutline[g] /= 64.0f;
		}

		charOutlines.contours.push_back(ofTTFContour());

		if( testOutline.size() ){
			//	charOutlines.contours.back().pts = ofSimplifyContour(testOutline, (float)TTF_SHAPE_SIMPLIFICATION_AMNT);
		}else{
			charOutlines.contours.back().pts = testOutline;
		}
	}

	return charOutlines;
}

//------------------------------------------------------------------
urFont::urFont(){
	bLoadedOk		= false;
	bMakeContours	= false;
	refCount		= 0;
}

//------------------------------------------------------------------
urFont::~urFont(){
	if (bLoadedOk){
#ifdef FTGL
        if (font != NULL )
            delete font;
        if (atlas != NULL )
            delete atlas;
    }
#else
		if (cps != NULL){
			delete[] cps;
		}
		if (texNames != NULL){
			for (int i = 0; i < nCharacters; i++){
				glDeleteTextures(1, &texNames[i]);
			}
			delete[] texNames;
		}
	}
#endif
}

//------------------------------------------------------------------
void urFont::loadFont(string filename, int fontsize){
	// load anti-aliased, non-full character set:
	loadFont(filename, fontsize, true, false, false);
}

#ifdef FTGL
extern string errorfontPath;
#endif

//------------------------------------------------------------------
void urFont::loadFont(string filename, int fontsize, bool _bAntiAliased, bool _bFullCharacterSet, bool makeContours, bool contourThickness){
	
	bMakeContours = makeContours;
    
	//------------------------------------------------
	if (bLoadedOk == true){
#ifdef FTGL
        if (font != NULL)
            delete font;
        if (atlas != NULL)
        {
            glDeleteTextures(1, &atlas->id);
            delete atlas;
        }
#else
		// we've already been loaded, try to clean up :

		if (cps != NULL){
			delete[] cps;
		}
		if (texNames != NULL){
			for (int i = 0; i < nCharacters; i++){
				glDeleteTextures(1, &texNames[i]);
			}
			delete[] texNames;
		}
#endif
		bLoadedOk = false;
	}
	//------------------------------------------------


//	filename = ofToDataPath(filename);

	bLoadedOk 			= false;
	bAntiAlised 		= _bAntiAliased;
	bFullCharacterSet 	= _bFullCharacterSet;
	fontSize			= fontsize;
    linegapscaling      = 1.43;

#ifdef FTGL
    font = 0;
    atlas = texture_atlas_new( 512, 512, 2 );
    font = texture_font_new( atlas, filename.c_str(), fontsize*10/7 );
   if(font == NULL)
        font = texture_font_new( atlas, errorfontPath.c_str(), fontsize*10/7 );
    font->outline_type = makeContours;
    font->outline_thickness = contourThickness;

#ifdef FTOUTLINE
    if(makeContours)
    {
        outlineatlas = texture_atlas_new( 512, 512, 2 );
        outlinefont = texture_font_new( outlineatlas, filename.c_str(), fontsize*10/7 );
    }
#endif
#else
	//--------------- load the library and typeface
	FT_Library library;
	if (FT_Init_FreeType( &library )){
		//		ofLog(OF_LOG_ERROR," PROBLEM WITH FT lib");
		return;
	}

//    NSLog(@"Font loading: %s",filename.c_str());
	FT_Face face;
	if (FT_New_Face( library, filename.c_str(), 0, &face )) {
		return;
	}
	FT_Set_Char_Size( face, fontsize << 6, fontsize << 6, 96, 96);
#endif
	lineHeight = fontsize * linegapscaling;

	//------------------------------------------------------
	//kerning would be great to support:
	//ofLog(OF_LOG_NOTICE,"FT_HAS_KERNING ? %i", FT_HAS_KERNING(face));
	//------------------------------------------------------

#ifdef FTGL
    texture_font_load_glyphs( font, L" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~" );
#ifdef FTOUTLINE
    texture_font_load_glyphs( outlinefont, L" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~" );
#endif

    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glBindTexture( GL_TEXTURE_2D, atlas->id );
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }    
    if (bAntiAlised == true){
        glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    }
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#ifndef OPENGLES2
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#endif
#else
	nCharacters = bFullCharacterSet ? 256 : 128 - NUM_CHARACTER_TO_START;

	//--------------- initialize character info and textures
	cps       = new charProps[nCharacters];
	texNames  = new GLuint[nCharacters];
    
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	glGenTextures(nCharacters, texNames);

    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	if(bMakeContours){
		charOutlines.clear();
		charOutlines.assign(nCharacters, ofTTFCharacter());
	}
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
	//--------------------- load each char -----------------------
	for (int i = 0 ; i < nCharacters; i++){
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }

		//------------------------------------------ anti aliased or not:
		if(FT_Load_Glyph( face, FT_Get_Char_Index( face, (unsigned char)(i+NUM_CHARACTER_TO_START) ), FT_LOAD_DEFAULT )){
			//			ofLog(OF_LOG_ERROR,"error with FT_Load_Glyph %i", i);
		}

        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		if (bAntiAlised == true) FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		else FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);

		//------------------------------------------
		FT_Bitmap& bitmap= face->glyph->bitmap;

		// 3 pixel border around the glyph
		// We show 2 pixels of this, so that blending looks good.
		// 1 pixels is hidden because we don't want to see the real edge of the texture

		border			= 3;
		visibleBorder	= 2;

		if(bMakeContours){
			if( printVectorInfo )printf("\n\ncharacter %c: \n", char( i+NUM_CHARACTER_TO_START ) );

			//int character = i + NUM_CHARACTER_TO_START;
			charOutlines[i] = makeContoursForCharacter( face );
		}

		// prepare the texture:
		int width  = ofNextPow2( bitmap.width + border*2 );
		int height = ofNextPow2( bitmap.rows  + border*2 );


		// ------------------------- this is fixing a bug with small type
		// ------------------------- appearantly, opengl has trouble with
		// ------------------------- width or height textures of 1, so we
		// ------------------------- we just set it to 2...
		if (width == 1) width = 2;
		if (height == 1) height = 2;

		// -------------------------
		// info about the character:
		cps[i].value 			= i;
		cps[i].height 			= face->glyph->bitmap_top;
		cps[i].width 			= face->glyph->bitmap.width;
		cps[i].setWidth 		= face->glyph->advance.x >> 6;
		cps[i].topExtent 		= face->glyph->bitmap.rows;
		cps[i].leftExtent		= face->glyph->bitmap_left;

		// texture internals
		cps[i].tTex             = (float)(bitmap.width + visibleBorder*2)  /  (float)width;
		cps[i].vTex             = (float)(bitmap.rows +  visibleBorder*2)   /  (float)height;

		cps[i].xOff             = (float)(border - visibleBorder) / (float)width;
		cps[i].yOff             = (float)(border - visibleBorder) / (float)height;


		/* sanity check:
		ofLog(OF_LOG_NOTICE,"%i %i %i %i %i %i",
		cps[i].value ,
		cps[i].height ,
		cps[i].width 	,
		cps[i].setWidth 	,
		cps[i].topExtent ,
		cps[i].leftExtent	);
		*/


		// Allocate Memory For The Texture Data.
		unsigned char* expanded_data = new unsigned char[ 2 * width * height];

		//-------------------------------- clear data:
		for(int j=0; j <height;j++) {
			for(int k=0; k < width; k++){
				expanded_data[2*(k+j*width)  ] = 255;   // every luminance pixel = 255
				expanded_data[2*(k+j*width)+1] = 0;
			}
		}


		if (bAntiAlised == true){
			//-----------------------------------
			for(int j=0; j <height; j++) {
				for(int k=0; k < width; k++){
					if ((k<bitmap.width) && (j<bitmap.rows)){
						expanded_data[2*((k+border)+(j+border)*width)+1] = bitmap.buffer[k + bitmap.width*(j)];
					}
				}
			}
			//-----------------------------------
		} else {
			//-----------------------------------
			// true type packs monochrome info in a
			// 1-bit format, hella funky
			// here we unpack it:
			unsigned char *src =  bitmap.buffer;
			for(int j=0; j <bitmap.rows;j++) {
				unsigned char b=0;
				unsigned char *bptr =  src;
				for(int k=0; k < bitmap.width ; k++){
					expanded_data[2*((k+1)+(j+1)*width)] = 255;
					if (k%8==0){ b = (*bptr++);}
					expanded_data[2*((k+1)+(j+1)*width) + 1] =
						b&0x80 ? 255 : 0;
					b <<= 1;
				}
				src += bitmap.pitch;
			}
			//-----------------------------------
		}


		//Now we just setup some texture paramaters.
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glBindTexture( GL_TEXTURE_2D, texNames[i]);
#if (!defined TARGET_IPHONE) && (!defined TARGET_ANDROID)
        /*
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
         */
#endif
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		if (bAntiAlised == true){
			glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		} else {
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		}
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#ifndef OPENGLES2
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		//Here we actually create the texture itself, notice
		//that we are using GL_LUMINANCE_ALPHA to indicate that
		//we are using 2 channel data.

#if (!defined TARGET_IPHONE) && (!defined TARGET_ANDROID) // gluBuild2DMipmaps doesn't seem to exist in anything i had in the iphone build... so i commented it out
/*
 bool b_use_mipmaps = false;  // FOR now this is fixed to false, could be an option, left in for legacy...
		if (b_use_mipmaps){
			gluBuild2DMipmaps(
				GL_TEXTURE_2D, GL_LUMINANCE_ALPHA, width, height,
				GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, expanded_data);
		} else
 */
#endif
		{
			glTexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, width, height,
				0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, expanded_data );
		}


		//With the texture created, we don't need to expanded data anymore

		delete [] expanded_data;

	}
#endif

#ifndef FTGL
	// ------------- close the library and typeface
	FT_Done_Face(face);
	FT_Done_FreeType(library);
#endif
	bLoadedOk = true;
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
}

//-----------------------------------------------------------
int urFont::ofNextPow2 ( int a )
{
	int rval=1;
	while(rval<a) rval<<=1;
	return rval;
}

float urFont::getLineWidth(const char *line) {
	if(!bLoadedOk) return 0.0f;
	float width=0.0f;
#ifdef FTGL
    texture_glyph_t *glyph;
#endif
	for(int i=0;line[i];i++) {
#ifdef FTGL
        glyph = texture_font_get_glyph( font, line[i] );
        width += glyph->advance_x;
#else
		int cy = (unsigned char)line[i] - NUM_CHARACTER_TO_START;
		if(cy >= 0 && cy < nCharacters)
			width+=cps[cy].setWidth;
#endif
	}
	return width;
}


#ifdef FTGL
int urFont::drawChar(int c, float x, float y, int kerningchar) {
	if (!bLoadedOk){
		//		ofLog(OF_LOG_ERROR,"Error : font not allocated -- line %d in %s", __LINE__,__FILE__);
		return 0;
	}
    
    texture_glyph_t *glyph = texture_font_get_glyph( font, c );
    int w=0;
    if( glyph != NULL )
    {
        int kerning = texture_glyph_get_kerning( glyph, kerningchar );
        w += kerning;
        x += kerning;
        int x1  = (int)( x + glyph->offset_x );
        int y1  = (int)( y + glyph->offset_y );
        int x2  = (int)( x1 + glyph->width );
        int y2  = (int)( y1 - glyph->height );
        float t1 = glyph->s0;
        float v1 = glyph->t0;
        float t2 = glyph->s1;
        float v2 = glyph->t1;

        GLfloat verts[] = { x1,y1,
			x1, y2,
			x2, y2,
			x2, y1 };
		GLfloat tex_coords[] = { t1, v1,
			t1, v2,
			t2, v2,
			t2, v1 };
/*
        GLfloat verts[] = { x2,y2,
			x2, y1,
			x1, y1,
			x1, y2 };
		GLfloat tex_coords[] = { t2, v2,
			t2, v1,
			t1, v1,
			t1, v2 };
*/
/*
#ifdef OPENGLES2
        urPushMatrix();
        urRotatef(180,1,0,0);
        GLenum err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        
#else
        glPushMatrix();
        glRotatef(180,1,0,0);
#endif
 */
        GLenum err = glGetError();
#ifdef OPENGLES2
        glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
        glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, GL_FALSE, 0, tex_coords);
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, verts);
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#else
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glTexCoordPointer(2, GL_FLOAT, 0, tex_coords );
		glEnableClientState( GL_VERTEX_ARRAY );
		glVertexPointer(2, GL_FLOAT, 0, verts );
#endif
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#ifndef OPENGLES2
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
#endif

        w += glyph->advance_x;
        /*
#ifdef OPENGLES2
        urPopMatrix();
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        
#else
        glPopMatrix();
#endif
*/
        return w;
    }
    return 0;
}
#else

//-----------------------------------------------------------
void urFont::drawChar(int c, float x, float y, int kerningchar) {
	if (!bLoadedOk){
		//		ofLog(OF_LOG_ERROR,"Error : font not allocated -- line %d in %s", __LINE__,__FILE__);
		return;
	}
    
	if (c >= nCharacters){
		//ofLog(OF_LOG_ERROR,"Error : char (%i) not allocated -- line %d in %s", (c + NUM_CHARACTER_TO_START), __LINE__,__FILE__);
		return;
	}
    
	int cu = c;
    
	GLint height	= cps[cu].height;
	GLint bwidth	= cps[cu].width;
	GLint top		= cps[cu].topExtent - cps[cu].height;
	GLint lextent	= cps[cu].leftExtent;
    
	GLfloat	x1, y1, x2, y2, corr, stretch;
	GLfloat t1, v1, t2, v2;
    
	//this accounts for the fact that we are showing 2*visibleBorder extra pixels
	//so we make the size of each char that many pixels bigger
	stretch = (float)(visibleBorder * 2);
    
	t2		= cps[cu].xOff;
	v2		= cps[cu].yOff;
	t1		= cps[cu].tTex + t2;
	v1		= cps[cu].vTex + v2;
    
	corr	= (float)(( (fontSize - height) + top) - fontSize);
    
	x1		= lextent + bwidth + stretch;
	y1		= height + corr + stretch;
	x2		= (float) lextent;
	y2		= -top + corr;
#ifdef OPENGLES2
	urPushMatrix();
	urRotatef(180,1,0,0);
#else
	glPushMatrix();
	glRotatef(180,1,0,0);
#endif
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
	if (glIsTexture(texNames[cu])) {
		glBindTexture(GL_TEXTURE_2D, texNames[cu]);
#ifndef OPENGLES2
		glNormal3f(0, 0, 1);
#endif
        
		GLfloat verts[] = { x2,y2,
			x2, y1,
			x1, y1,
			x1, y2 };
		GLfloat tex_coords[] = { t2, v2,
			t2, v1,
			t1, v1,
			t1, v2 };
        
#ifdef OPENGLES2
        glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
        glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, GL_FALSE, 0, tex_coords);
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, verts);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#else
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glTexCoordPointer(2, GL_FLOAT, 0, tex_coords );
		glEnableClientState( GL_VERTEX_ARRAY );
		glVertexPointer(2, GL_FLOAT, 0, verts );
#endif
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#ifndef OPENGLES2
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
#endif
        
	} else {
		//let's add verbosity levels somewhere...
		//this error, for example, is kind of annoying to see
		//all the time:
		//		ofLog(OF_LOG_WARNING," texture not bound for character -- line %d in %s", __LINE__,__FILE__);
	}
#ifdef OPENGLES2
	urPopMatrix();
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
#else
	glPopMatrix();
#endif
}
#endif

int char2col(char c)
{
    if(c>='0' && c<='9')
    {
        return c-'0';
    }
    if(c>='A' && c<='F')
    {
        return c-'A'+10;
    }
    if(c>='a' && c<='f')
    {
        return c-'a'+10;
    }

    return -1;
}

inline int chars2col(char *c)
{
    return char2col(c[0])*16+char2col(c[1]);
}

extern GLubyte squareColors[];

/*
#ifdef OPENGLES2
GLubyte squareColors[] = {
    //    GLfloat squareColors[] = {
    255, 255,   0, 255,
    0,   255, 255, 255,
    0,     0,   0,   0,
    255,   0, 255, 255,
};
#else
GLubyte squareColors[] = {
    255, 255,   0, 255,
    0,   255, 255, 255,
    0,     0,   0,   0,
    255,   0, 255, 255,
};
#endif
*/

int urFont::parseColor(const string &c,int index)
{
    for(int i=0; i<8; i++)
        if(c[i]=='\0')
            return i;
    int r,g,b,a;
    r = char2col(c[index])*16+char2col(c[index+1]);
    g = char2col(c[index+2])*16+char2col(c[index+3]);
    b = char2col(c[index+4])*16+char2col(c[index+5]);
    a = char2col(c[index+6])*16+char2col(c[index+7]);
    
    for(int i=0;i<4;i++)
    {
        squareColors[4*i] = r;
        squareColors[4*i+1] = g;
        squareColors[4*i+2] = b;
        squareColors[4*i+3] = a;
    }
#ifdef OPENGLES2
    glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, squareColors);
    glEnableVertexAttribArray(ATTRIB_COLOR);
#else
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
    glEnableClientState(GL_COLOR_ARRAY);
#endif
    
    return index + 7;
}

void urFont::resetColor()
{
    for(int i=0;i<4;i++)
    {
        squareColors[4*i] = 255;
        squareColors[4*i+1] = 255;
        squareColors[4*i+2] = 255;
        squareColors[4*i+3] = 255;
    }
#ifdef OPENGLES2
    glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, squareColors);
    glEnableVertexAttribArray(ATTRIB_COLOR);
#else
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
    glEnableClientState(GL_COLOR_ARRAY);
#endif
}

//=====================================================================
charPos* urFont::drawString(string c, float x, float y) {
	
	if (!bLoadedOk){
		//    	ofLog(OF_LOG_ERROR,"Error : font not allocated -- line %d in %s", __LINE__,__FILE__);
		return NULL;
	};

	GLint		index	= 0;
	GLfloat		X		= 0;
	GLfloat		Y		= 0;

	// (a) record the current "alpha state, blend func, etc"
#if (!defined TARGET_IPHONE) && (!defined TARGET_ANDROID)
//	glPushAttrib(GL_COLOR_BUFFER_BIT);
#else
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	GLboolean blend_enabled = glIsEnabled(GL_BLEND);
#ifdef CACHESTRINGTEXTURE
    GLboolean scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
#endif
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
//	GLboolean texture_2d_enabled = glIsEnabled(GL_TEXTURE_2D);
	GLint blend_src, blend_dst;
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	glGetIntegerv( GL_BLEND_SRC, &blend_src );
	glGetIntegerv( GL_BLEND_DST, &blend_dst );
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#endif

	// (b) enable our regular ALPHA blending!
	glEnable(GL_BLEND);
//    glBlendFunc(GL_SRC_ALPHA, GL_ZERO);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
#ifdef CACHESTRINGTEXTURE
    glDisable(GL_SCISSOR_TEST);
#endif
	// (c) enable texture once before we start drawing each char (no point turning it on and off constantly)
#ifdef OPENGLES2
    glActiveTexture(GL_TEXTURE0);
#else
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	glEnable(GL_TEXTURE_2D);
#endif
	// (d) store the current matrix position and do a translation to the drawing position
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#ifdef OPENGLES2
    urPushMatrix();
//    urLoadIdentity();
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    urTranslatef(x, y, 0);
  
#ifdef FTGL
    glBindTexture( GL_TEXTURE_2D, atlas->id );
#endif
    
	int len = (int)c.length();
    
    charPos* returnpos = new charPos[len+1];
    int ylinegap = (lineHeight - fontSize)/2.0;
    int returnposy = Y - ylinegap;
    int kerningchar = 0;
	while(index < len){
        returnpos[index].value = c[index];
        returnpos[index].x = X+x;
        returnpos[index].y = returnposy+y;
        returnpos[index].len = len;
#ifdef FTGL
        returnpos[index].height = lineHeight;
        if (c[index] == '\n') {
            Y = (float) lineHeight;
            returnposy = returnposy - Y;
            urTranslatef(-X, -Y, 0);
            X = 0 ; //reset X Pos back to zero
            kerningchar = 0;
        } else {

            if ( c[index] != '|' || (c[index] == '|' && (c[index+1] != 'c' && c[index+1] != 'r')))
            {
                int width = drawChar(c[index], 0, 0, kerningchar);
                kerningchar = c[index];
                urTranslatef((float)width, 0, 0);
                returnpos[index].width = width;
                
                if ( c[index] == '|' && c[index+1] == '|')
                {
                    index++;
                    returnpos[index].value = c[index+1];
                    returnpos[index].x = X+x;
                    returnpos[index].y = returnposy+y;
                    returnpos[index].len = len;
                    returnpos[index].width = 0; // Specials don't get width
                }
                X += width;
            }
            else
            {
                if(c[index+1] == 'r')
                {
                    returnpos[index].width = 0; // Specials don't get width
                    resetColor();
                    index++;
                    returnpos[index].value = c[index];
                    returnpos[index].x = X+x;
                    returnpos[index].y = returnposy+y;
                    returnpos[index].len = len;
                    returnpos[index].width = 0; // Specials don't get width
                }
                else
                {
                    returnpos[index].width = 0; // Specials don't get width
                    int oldindex = index;
                    index = parseColor(c,index+2); // Color syntax is |cRRGGBBAA so we skip the implied c here.
                    for(int i=oldindex+1;i<=index;i++)
                    {
                        returnpos[i].value = c[i];
                        returnpos[i].x = X+x;
                        returnpos[i].y = returnposy+y;
                        returnpos[i].len = len;
                        returnpos[i].width = 0; // Specials don't get width
                    }
                }
            }
        }
    
#else
		int cy = (unsigned char)c[index] - NUM_CHARACTER_TO_START;
		if (cy < nCharacters){ 			// full char set or not?
            returnpos[index].height = lineHeight;
			if (c[index] == '\n') {
                
				Y = (float) lineHeight;
				urTranslatef(-X, -Y, 0);
				X = 0 ; //reset X Pos back to zero

			}else if (c[index] == ' ') {
				int cy = (int)'p' - NUM_CHARACTER_TO_START;
				X += cps[cy].width;
				urTranslatef((float)cps[cy].width, 0, 0);
                returnpos[index].width = cps[cy].width;
			} else {
				drawChar(cy, 0, 0, 0);
				X += cps[cy].setWidth;
				urTranslatef((float)cps[cy].setWidth, 0, 0);
                returnpos[index].width = cps[cy].setWidth;
 			}
        }
#endif
		index++;
	}
    returnpos[index].value = '\0';
    returnpos[index].x = X+x;
    returnpos[index].y = returnposy+y;
    returnpos[index].width = 0;
    returnpos[index].height = lineHeight;
    returnpos[index].len = len;
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
    urPopMatrix();
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

#else
	glPushMatrix();
	glTranslatef(x, y, 0);
	//glRotatef(180,0,0,1);

	int len = (int)c.length();

    charPos* returnpos = new charPos[len+1];
    
	while(index < len){
        returnpos[index].value = c[index];
        returnpos[index].x = X;
        returnpos[index].y = Y;
		int cy = (unsigned char)c[index] - NUM_CHARACTER_TO_START;
		if (cy < nCharacters){ 			// full char set or not?
			if (c[index] == '\n') {

				Y = (float) lineHeight;
				glTranslatef(-X, -Y, 0);
				X = 0 ; //reset X Pos back to zero

			}else if (c[index] == ' ') {
				int cy = (int)'p' - NUM_CHARACTER_TO_START;
				X += cps[cy].width;
				glTranslatef((float)cps[cy].width, 0, 0);
                returnpos[index].width = cps[cy].width;
			} else {
				drawChar(cy, 0, 0, 0);
				X += cps[cy].setWidth;
				glTranslatef((float)cps[cy].setWidth, 0, 0);
                returnpos[index].width = cps[cy].setWidth;
			}
		}
		index++;
	}
    returnpos[index].value = '\0';
    returnpos[index].x = X;
    returnpos[index].y = Y;
    returnpos[index].width = 0;

	glPopMatrix();
	glDisable(GL_TEXTURE_2D);
#endif
	// (c) return back to the way things were (with blending, blend func, etc)
#if (!defined TARGET_IPHONE) && (!defined TARGET_ANDROID)
    /*
	glPopAttrib();
     */
#else
	if( !blend_enabled )
		glDisable(GL_BLEND);
//	if( !texture_2d_enabled )
//		glDisable(GL_TEXTURE_2D);
	glBlendFunc( blend_src, blend_dst );
#ifdef CACHESTRINGTEXTURE
    if ( scissor_enabled )
        glEnable(GL_SCISSOR_TEST);
#endif
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#endif
    return returnpos;
}

#endif