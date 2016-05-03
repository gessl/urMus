#include "config.h"
#ifndef UISTRINGS
#include "urTexture.h"

// static font storage
#include <map>
std::map<string,urFont *> fonts;

#include "config.h"

// Replaces '/n' with '/0' and returns the remaining string after. Returns NULL if the no more strings are found.
char *lineizer(char *s, bool &subst)
{
    char *p = s;
    if(p == NULL || (*p == '\0' && subst == false))
        return NULL;

    while(*p && *p!='\n')
    {
        p++;
    }
    if(*p == '\n')
    {
        subst = true;
        *p = '\0';
        p++;
        return p;
    }
    subst = false;
    return p;
}


#ifdef OPENGLES2
enum { ATTRIB_VERTEX, ATTRIB_COLOR, ATTRIB_TEXTUREPOSITON, NUM_ATTRIBUTES };
#endif

//#ifndef UISTRINGS
void urPushMatrix();
void urPopMatrix();
void urTranslatef(GLfloat x, GLfloat y, GLfloat z);
void urRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void urScalef(GLfloat sx, GLfloat sy, GLfloat sz);
//#endif

inline int pow2roundup (int x)
{
    if (x < 0) return 0;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}

urTexture::urTexture(const void *data, GLenum format, unsigned int width, unsigned int height)
{
/*	GLint saveName;
	
	glGenTextures(1, &name);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &saveName);
	glBindTexture(GL_TEXTURE_2D, name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, saveName);*/

	this->width=width;
	this->height=height;
	this->format=format;
	
	this->font=NULL;
	this->bAutoTexCoord=false;
}

urTexture::urTexture(unsigned int width, unsigned int height)
{
	this->name=-1;
	this->width=width;
	this->height=height;
	
	this->font=NULL;
	this->bAutoTexCoord=false;
}

urTexture::urTexture(urImage *image) 
{
	GLint saveName;
	
	int internalFormat=1;
	switch(image->getColorType()) {
		case PNG_COLOR_TYPE_RGB:
			format=GL_RGB;
			internalFormat=GL_RGB;
			break;
		case PNG_COLOR_TYPE_RGBA:
			format=GL_RGBA;
			internalFormat=GL_RGBA;
			break;
		case PNG_COLOR_TYPE_BGRA:
			format=GL_BGRA;
			internalFormat=GL_RGBA;
			//image->flipPixels();
			break;
	}
	
	width=image->getWidth();
	height=image->getHeight();
	texWidth=pow2roundup(width);
	texHeight=pow2roundup(height);

	image->resize(texWidth, texHeight);
	const void* data=image->getBuffer();
	
	_maxS=(GLfloat)width/texWidth;
	_maxT=(GLfloat)height/texHeight;
	

	glGenTextures(1, &name);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &saveName);
	glBindTexture(GL_TEXTURE_2D, name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texWidth, texHeight, 0, format, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, saveName);

	this->format=format;
	
	this->font=NULL;
	this->bAutoTexCoord=false;
}

urTexture::urTexture(const char *str, const char *fontname, unsigned int size, unsigned int width, unsigned int height,
					 UITextAlignment alignment, UILineBreakMode mode, CGSize shadowOffset, GLfloat shadowBlur, GLfloat shadowColor[], int outlinemode, int outlinethickness, GLfloat outlinecolor[]) {
	name=0;

	char fontattrstr[16];
	sprintf(fontattrstr, " %d-%d-%d", size,outlinemode, outlinethickness);
	string key=fontname;
	key += fontattrstr;
	if(fonts.find(key)==fonts.end()) {
		fonts[key]=font=new urFont();
		font->loadFont(fontname,size*7/10,true,outlinemode,outlinethickness);
		if(font->bLoadedOk==false)
			fprintf(stderr,"Warning : font loading fail - %s",key.c_str());
		
		font->key=key;
		font->refCount++;
	} else {
		font=fonts[key];
		font->refCount++;
	}
	
	this->width=width;
	this->height=height;
	this->str=str;

    yalign = 0;
	
	texWidth=pow2roundup(width);
	texHeight=pow2roundup(height);
	
	_maxS=(GLfloat)width/texWidth;
	_maxT=(GLfloat)height/texHeight;
	
	this->alignment=alignment;
	this->linebreakmode=mode;
	this->shadowOffset=shadowOffset;
	this->shadowBlur=0;//shadowBlur;
	this->shadowColor[0]=(shadowColor)?shadowColor[0]:0;
	this->shadowColor[1]=(shadowColor)?shadowColor[1]:0;
	this->shadowColor[2]=(shadowColor)?shadowColor[2]:0;
	this->shadowColor[3]=(shadowColor)?shadowColor[3]:0;
	
    char *copy=new char[this->str.length()+1];
    strcpy(copy, str);
    char *line = copy;
    bool subst = true;
    char *remainder = lineizer(line, subst);
    
    int totallineheight = 0;
    int lineheight = font->getLineHeight();
    do { // Only print as many lines as will be visible
		lines.push_back(line);
		widths.push_back(font->getLineWidth(line));//+5);
        totallineheight += lineheight;
        
        line = remainder;
        if(line != NULL)
        {
            remainder = lineizer(line, subst);
        }
	}
    while(remainder!=NULL && totallineheight <= height);
    charPosLines = new charPos*[lines.size()];
	delete [] copy;
	
#ifdef CACHESTRINGTEXTURE
	GLuint oldFbo, fbo, saveName;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_OES, (GLint*)&oldFbo);
	glGenFramebuffersOES(1, &fbo);
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, fbo);

	glGenTextures(1, &name);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&saveName);
	glBindTexture(GL_TEXTURE_2D, name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, name, 0);

    GLenum errf = glCheckFramebufferStatus(GL_FRAMEBUFFER_OES);
    if(errf!= GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"Frame buffer incomplete: %d",errf);
        int a=0;
    }
	glEnable(GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
//    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

#ifdef UISTRINGS
	glEnableClientState(GL_COLOR_ARRAY);
	glPushMatrix();
//	glTranslatef(0.0f, texHeight, 0.0f);
//	glScalef(1.0f, -1.0f, 1.0f);
#else
    urPushMatrix();
//	urTranslatef(0.0f, texHeight, 0.0f);
//	urScalef(1.0f, -1.0f, 1.0f);
#endif
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//	renderString(CGRectMake(0, texHeight-height, width, height));
	renderString(CGRectMake(0, 0, width, height));
#ifdef UISTRINGS
	glPopMatrix();
#else
    urPopMatrix();
#endif
	
	glBindTexture(GL_TEXTURE_2D, saveName);
	glDeleteFramebuffersOES(1, &fbo);
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, oldFbo);
#endif
}

urTexture::~urTexture(void)
{
	if(name)
		glDeleteTextures(1, &name);
	
/*	if(font) {
		font->refCount--;
		if(font->refCount==0) {
			fonts.erase(font->key);
			delete font;
		}
	}*/
}


void urTexture::drawInRect(CGRect rect) {
	if(name) {	// it's an image
		GLfloat  coordinates[] = { 0,_maxT, _maxS,_maxT, 0,0, _maxS,0};
		GLfloat vertices[] = {  rect.origin.x, rect.origin.y, 0.0,
			rect.origin.x + rect.size.width, rect.origin.y, 0.0,
			rect.origin.x, rect.origin.y + rect.size.height, 0.0,
		rect.origin.x + rect.size.width, rect.origin.y + rect.size.height, 0.0 };
	
		GLint saveName;
		if(name>0) {
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &saveName);
			glBindTexture(GL_TEXTURE_2D, name);
		}
		glVertexPointer(3, GL_FLOAT, 0, vertices);
		if(bAutoTexCoord)
			glTexCoordPointer(2, GL_FLOAT, 0, coordinates);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		if(name>0) {
			glBindTexture(GL_TEXTURE_2D, saveName);
		}
	} else if(font && font->bLoadedOk) {	// it's a text
		renderString(rect);
	}
}

void urTexture::setYAlign(GLint y)
{
    yalign = y + height/2.0;
}

void urTexture::drawAtPoint(CGPoint point, bool tile) {
	if(name) {	// it's an image or cashed font
        
        /*
		GLfloat	coordinates[] = { 0,_maxT, _maxS,_maxT, 0,0, _maxS,0};
		GLfloat	vertices[] = {  point.x,			point.y,			0.0,
								width + point.x,	point.y,			0.0,
								point.x,			height + point.y,	0.0,
								width + point.x,	height + point.y,	0.0  };
         */
		GLfloat	coordinates[] = { 0,0, _maxS,0, 0,_maxT, _maxS,_maxT};
		GLfloat	vertices[] = {  point.x,			point.y,			0.0,
            width + point.x,	point.y,			0.0,
            point.x,			height + point.y,	0.0,
            width + point.x,	height + point.y,	0.0  };
		
		GLint saveName;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &saveName);
		glBindTexture(GL_TEXTURE_2D, name);
#ifdef OPENGLES2
        glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
        glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, GL_FALSE, 0, coordinates);
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
#else
		glVertexPointer(3, GL_FLOAT, 0, vertices);
		glTexCoordPointer(2, GL_FLOAT, 0, coordinates);
#endif
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindTexture(GL_TEXTURE_2D, saveName);
	} else if(font && font->bLoadedOk) {	// it's a text
		renderString(CGRectMake(point.x,point.y,width,height));
	}
}

charPos** urTexture::getCharPosLines()
{
    return charPosLines;
}

int urTexture::charIndexAtPos(int x, int y)
{

    if(charPosLines == NULL || charPosLines[0] == NULL)
        return -1;
    int tidx=0;
    for(int i=0;i<lines.size();i++) {
        charPos* linepos = charPosLines[i];
        int idx = 0;
        int cy = linepos[idx].y + yalign;
        int cy2 = cy + linepos[idx].height;
        int cx, cx2;
//        if (y >= cy && y < cy2) // Inside the line
        {
            while(linepos[idx].value != '\0') // should be binary search but that requires length of lines...
            {
                cx = linepos[idx].x;
                
                cx2 = cx + linepos[idx].width;
                if( x > cx && x < cx2 && y > cy && y < cy2 && linepos[idx].width > 0)
                {
                    char a = linepos[idx].value;
                    NSLog(@"Char at: %d %d: %d(%d)", x,y, linepos[idx].value, linepos[idx].width);

                    return tidx;
                }
                idx++;
                tidx++;
            }
            if (y > cy && y < cy2)
            {
                NSLog(@"Char at newline: %d %d: %c(%d)", x,y, linepos[idx].value, linepos[idx].width);
                return tidx; // Return end of line
            }
        }
        tidx++; // For newline
    }
    return -1; // Not found
}

void urTexture::drawString(CGRect rect) {
	float lineHeight=font->getLineHeight();
    float lineGap = font->getLineGap();
	float width=rect.size.width;
	float height=rect.size.height;
	int nLines=lines.size();
	for(int i=0;i<nLines;i++) {
//		float offsetX, offsetY=offsetY=height-lineHeight*i-lineHeight+3;// Constant is hacky but works, should be derived from font //(height+lineHeight*(nLines-2*i-2))*0.5;
		float offsetX, offsetY=offsetY=lineHeight*(nLines-i-1)+lineGap;// Constant is hacky but works, should be 		if(alignment==UITextAlignmentLeft)
			offsetX=0;
		if(alignment==UITextAlignmentCenter) 
			offsetX=(width-widths[i])*0.5;
		if(alignment==UITextAlignmentRight) 
			offsetX=width-widths[i];
		charPosLines[i]=font->drawString(lines[i], rect.origin.x+offsetX, offsetY+rect.origin.y);
        charPosLines[i]->line=i;
	}
}

void urTexture::renderString(CGRect rect) {
/*
	if(shadowBlur>0.0f) {
		float color[4];
		glGetFloatv(GL_CURRENT_COLOR, color);
		int repeat=ceil(shadowBlur);
			//	GLubyte shadowColors[]={shadowColor[0],shadowColors[1],shadowColors[2],shadowColors[3]*alpha,
	//		shadowColor[0],shadowColors[1],shadowColors[2],shadowColors[3]*alpha,
	//		shadowColor[0],shadowColors[1],shadowColors[2],shadowColors[3]*alpha,
	//		shadowColor[0],shadowColors[1],shadowColors[2],shadowColors[3]*alpha};
		
		
		for(int i=-repeat;i<=repeat;i++) {
			for(int j=-repeat;j<=repeat;j++) {
//				GLubyte alpha=255/((ABS(i)+ABS(j))/2+1);
//				GLubyte shadowColors[]={0,0,0,alpha,0,0,0,alpha,0,0,0,alpha,0,0,0,alpha};
//				glColorPointer(4, GL_UNSIGNED_BYTE, 0, shadowColors);
				glPushMatrix();
				glTranslatef(i, j, 0);
				drawString(rect);
				glPopMatrix();
			}
		}
	}
//	GLubyte squareColors[]={255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
//	glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
 */
	drawString(rect);
}
#endif

