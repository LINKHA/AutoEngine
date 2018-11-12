#include "TextureSpace.h"
#include "Application.h"
#include "Level_0.h"

TextureSpace::TextureSpace(Ambient* ambient)
	:MotionSpace(ambient)
{
}
TextureSpace::~TextureSpace()
{
	RemoveScene(0);
}

void TextureSpace::Awake()
{
	RegisterScene(new Level_0(_ambient, 0));
}
int TextureSpace::Launch()
{
	return INSTANCE(Application).Run(_ambient);
}
AUTO_APPLICATION_MAIN(TextureSpace)
