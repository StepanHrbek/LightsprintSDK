// --------------------------------------------------------------------------
// Scene viewer - custom properties for property grid.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVCustomProperties.h"


//////////////////////////////////////////////////////////////////////////////
//
// FloatProperty

FloatProperty::FloatProperty(const wxString& label, const wxString& help, float value, int precision, float mini, float maxi, float step, bool wrap)
	: wxFloatProperty(label,wxPG_LABEL,value)
{
	step *= 0.1f;
	SetAttribute("Precision",precision);
	SetAttribute("Min",mini);
	SetAttribute("Max",maxi);
	SetAttribute("Step",step);
	SetAttribute("Wrap",wrap);
	SetAttribute("MotionSpin",true);
	SetEditor("SpinCtrl");
	SetHelpString(help);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRVec2Property

WX_PG_IMPLEMENT_VARIANT_DATA_DUMMY_EQ(RRVec2)
WX_PG_IMPLEMENT_PROPERTY_CLASS(RRVec2Property,wxPGProperty,RRVec2,const RRVec2&,TextCtrl)

RRVec2Property::RRVec2Property( const wxString& label, const wxString& help, int precision, const RRVec2& value, float step )
	: wxPGProperty(label,wxPG_LABEL)
{
	step *= 0.1f;
	SetValue( WXVARIANT(value) );
	SetHelpString(help);
	AddPrivateChild(new FloatProperty("x","",value.x,precision,-1e10f,1e10f,step,false));
	AddPrivateChild(new FloatProperty("y","",value.y,precision,-1e10f,1e10f,step,false));
}

void RRVec2Property::RefreshChildren()
{
	if ( !GetChildCount() ) return;
	const RRVec2& vector = RRVec2RefFromVariant(m_value);
	Item(0)->SetValue( vector.x );
	Item(1)->SetValue( vector.y );
}

wxVariant RRVec2Property::ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const
{
	RRVec2 vector;
	vector << thisValue;
	switch ( childIndex )
	{
		case 0: vector.x = childValue.GetDouble(); break;
		case 1: vector.y = childValue.GetDouble(); break;
	}
	thisValue << vector;
	return thisValue;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRVec3Property

WX_PG_IMPLEMENT_VARIANT_DATA_DUMMY_EQ(RRVec3)
WX_PG_IMPLEMENT_PROPERTY_CLASS(RRVec3Property,wxPGProperty,RRVec3,const RRVec3&,TextCtrl)

RRVec3Property::RRVec3Property( const wxString& label, const wxString& help, int precision, const RRVec3& value, float step )
	: wxPGProperty(label,wxPG_LABEL)
{
	step *= 0.1f;
	SetValue( WXVARIANT(value) );
	SetHelpString(help);
	AddPrivateChild(new FloatProperty("x","",value.x,precision,-1e10f,1e10f,step,false));
	AddPrivateChild(new FloatProperty("y","",value.y,precision,-1e10f,1e10f,step,false));
	AddPrivateChild(new FloatProperty("z","",value.z,precision,-1e10f,1e10f,step,false));
}

void RRVec3Property::RefreshChildren()
{
	if ( !GetChildCount() ) return;
	const RRVec3& vector = RRVec3RefFromVariant(m_value);
	Item(0)->SetValue( vector.x );
	Item(1)->SetValue( vector.y );
	Item(2)->SetValue( vector.z );
}

wxVariant RRVec3Property::ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const
{
	RRVec3 vector;
	vector << thisValue;
	switch ( childIndex )
	{
		case 0: vector.x = childValue.GetDouble(); break;
		case 1: vector.y = childValue.GetDouble(); break;
		case 2: vector.z = childValue.GetDouble(); break;
	}
	thisValue << vector;
	return thisValue;
}


//////////////////////////////////////////////////////////////////////////////
//
// HDRColorProperty

WX_PG_IMPLEMENT_PROPERTY_CLASS(HDRColorProperty,wxPGProperty,RRVec3,const RRVec3&,TextCtrl)

HDRColorProperty::HDRColorProperty( const wxString& label, const wxString& help, int precision, const RRVec3& rgb )
	: wxPGProperty(label,wxPG_LABEL), image(2,1), bitmap(NULL)
{
	SetValue(WXVARIANT(rgb));
	SetHelpString(help);

	RRVec3 hsv = rgb.getHsvFromRgb();
	AddPrivateChild(new FloatProperty(_("red"),_("Red component intensity"),rgb[0],precision,0,100,0.1f,false));
	AddPrivateChild(new FloatProperty(_("green"),_("Green component intensity"),rgb[1],precision,0,100,0.1f,false));
	AddPrivateChild(new FloatProperty(_("blue"),_("Blue component intensity"),rgb[2],precision,0,100,0.1f,false));
	AddPrivateChild(new FloatProperty(_("hue"),_("Color hue, 0..360"),hsv[0],precision,0,360,10,true));
	AddPrivateChild(new FloatProperty(_("saturation"),_("Color saturation, 0..1"),hsv[1],precision,0,1,0.1f,false));
	AddPrivateChild(new FloatProperty(_("value"),_("Overall intensity"),hsv[2],precision,0,100,0.1f,false));
}

void HDRColorProperty::RefreshChildren()
{
	if ( !GetChildCount() ) return;
	const RRVec3& rgb = RRVec3RefFromVariant(m_value);
	RRVec3 hsv = rgb.getHsvFromRgb();
	Item(0)->SetValue(rgb[0]);
	Item(1)->SetValue(rgb[1]);
	Item(2)->SetValue(rgb[2]);
	Item(3)->SetValue(hsv[0]);
	Item(4)->SetValue(hsv[1]);
	Item(5)->SetValue(hsv[2]);

	// update ValueImage
	unsigned char* data = image.GetData();
	data[0] = data[3] = RR_FLOAT2BYTE(rgb[0]);
	data[1] = data[4] = RR_FLOAT2BYTE(rgb[1]);
	data[2] = data[5] = RR_FLOAT2BYTE(rgb[2]);
	delete bitmap;
	bitmap = new wxBitmap(image);
	SetValueImage(*bitmap);
}

wxVariant HDRColorProperty::ChildChanged( wxVariant& thisValue, int childIndex, wxVariant& childValue ) const
{
	RRVec3 rgb;
	rgb << thisValue;
	RRVec3 hsv = rgb.getHsvFromRgb();
	switch ( childIndex )
	{
		case 0: 
		case 1: 
		case 2:
			rgb[childIndex] = childValue.GetDouble();
			break;
		case 3: 
		case 4: 
		case 5:
			hsv[childIndex-3] = childValue.GetDouble();
			rgb = hsv.getRgbFromHsv();
			break;
	}
	thisValue << rgb;
	return thisValue;
}

HDRColorProperty::~HDRColorProperty()
{
	delete bitmap;
}


//////////////////////////////////////////////////////////////////////////////
//
// ImageFileProperty

ImageFileProperty::ImageFileProperty(const wxString& label, const wxString& help)
	: wxFileProperty(label)
{
	SetHelpString(help);
	image = NULL;
	bitmap = NULL;
}

ImageFileProperty::~ImageFileProperty()
{
	delete bitmap;
	delete image;
}

void ImageFileProperty::updateIcon(rr::RRBuffer* buffer)
{
	if (buffer && m_parentState) // be careful, wxWidgets SetValueImage would crash if m_parentState is NULL
	{
		wxSize size = wxSize(20,15);//GetImageSize();
		delete image;
		image = new wxImage(size);
		unsigned char* data = image->GetData();
		if (data)
		{
			for (int j=0;j<size.y;j++)
			for (int i=0;i<size.x;i++)
			{
				rr::RRVec4 rgba = buffer->getElementAtPosition(rr::RRVec3((i+0.45f)/size.x,(j+0.45f)/size.y,0));
				data[(i+j*size.x)*3+0] = RR_FLOAT2BYTE(rgba[0]);
				data[(i+j*size.x)*3+1] = RR_FLOAT2BYTE(rgba[1]);
				data[(i+j*size.x)*3+2] = RR_FLOAT2BYTE(rgba[2]);
			}
		}
		delete bitmap;
		bitmap = new wxBitmap(*image);
		SetValueImage(*bitmap);
	}
	else
		SetValueImage(*(wxBitmap*)NULL);
}

void ImageFileProperty::updateBufferAndIcon(rr::RRBuffer*& buffer, bool playVideos)
{
	if (buffer && GetValue().GetString()==buffer->filename.c_str())
	{
		RR_ASSERT(0); // inefficiency, loading the same buffer
	}

	if (buffer)
	{
		// stop it
		buffer->stop();
		delete buffer;
	}
	buffer = rr::RRBuffer::load(GetValue().GetString().c_str());
	if (buffer && playVideos)
		buffer->play();

	updateIcon(buffer);
}

wxString getTextureDescription(rr::RRBuffer* buffer)
{
	// must not contain <>*?/\, wxFileProperty would say "Validation conflict: '<no texture>' is invalid." on attempt to change texture
	return buffer
		? (buffer->filename.empty()
			? wxString::Format("(%dx%d embedded)",buffer->getWidth(),buffer->getHeight())
			: buffer->filename.c_str())
		:"(no texture)";
}


//////////////////////////////////////////////////////////////////////////////
//
// LocationProperty

struct CityLocation
{
	const wxChar* city;
	unsigned char latitudeDeg;
	unsigned char latitudeMin;
	unsigned char latitudeSign;
	unsigned char longitudeDeg;
	unsigned char longitudeMin;
	unsigned char longitudeSign;
	rr::RRVec2 getLatitudeLongitude() const
	{
		return rr::RRVec2(
			(latitudeDeg+latitudeMin/60.f)*((latitudeSign=='N')?1:-1),
			(longitudeDeg+longitudeMin/60.f)*((longitudeSign=='E')?1:-1)
			);
	}
};

const CityLocation cityLocations[] =
{
	{_("unknown"),0,0,'N',0,0,'W'},
	{wxT("Aberdeen, Scotland"),57,9,'N',2,9,'W'},
	{wxT("Adelaide, Australia"),34,55,'S',138,36,'E'},
	{wxT("Albany, N.Y., USA"),42,40,'N',73,45,'W'},
	{wxT("Albuquerque, N.M., USA"),35,5,'N',106,39,'W'},
	{wxT("Algiers, Algeria"),36,50,'N',3,0,'E'},
	{wxT("Amarillo, Tex., USA"),35,11,'N',101,50,'W'},
	{wxT("Amsterdam, Netherlands"),52,22,'N',4,53,'E'},
	{wxT("Anchorage, Alaska, USA"),61,13,'N',149,54,'W'},
	{wxT("Ankara, Turkey"),39,55,'N',32,55,'E'},
	{wxT("Asunción, Paraguay"),25,15,'S',57,40,'W'},
	{wxT("Athens, Greece"),37,58,'N',23,43,'E'},
	{wxT("Atlanta, Ga., USA"),33,45,'N',84,23,'W'},
	{wxT("Auckland, New Zealand"),36,52,'S',174,45,'E'},
	{wxT("Austin, Tex., USA"),30,16,'N',97,44,'W'},
	{wxT("Baker, Ore., USA"),44,47,'N',117,50,'W'},
	{wxT("Baltimore, Md., USA"),39,18,'N',76,38,'W'},
	{wxT("Bangkok, Thailand"),13,45,'N',100,30,'E'},
	{wxT("Bangor, Maine, USA"),44,48,'N',68,47,'W'},
	{wxT("Barcelona, Spain"),41,23,'N',2,9,'E'},
	{wxT("Beijing, China"),39,55,'N',116,25,'E'},
	{wxT("Belém, Brazil"),1,28,'S',48,29,'W'},
	{wxT("Belfast, Northern Ireland"),54,37,'N',5,56,'W'},
	{wxT("Belgrade, Serbia"),44,52,'N',20,32,'E'},
	{wxT("Berlin, Germany"),52,30,'N',13,25,'E'},
	{wxT("Birmingham, England"),52,25,'N',1,55,'W'},
	{wxT("Birmingham, Ala., USA"),33,30,'N',86,50,'W'},
	{wxT("Bismarck, N.D., USA"),46,48,'N',100,47,'W'},
	{wxT("Bogotá, Colombia"),4,32,'N',74,15,'W'},
	{wxT("Boise, Idaho, USA"),43,36,'N',116,13,'W'},
	{wxT("Bombay, India"),19,0,'N',72,48,'E'},
	{wxT("Bordeaux, France"),44,50,'N',0,31,'W'},
	{wxT("Boston, Mass., USA"),42,21,'N',71,5,'W'},
	{wxT("Bremen, Germany"),53,5,'N',8,49,'E'},
	{wxT("Brisbane, Australia"),27,29,'S',153,8,'E'},
	{wxT("Bristol, England"),51,28,'N',2,35,'W'},
	{wxT("Brussels, Belgium"),50,52,'N',4,22,'E'},
	{wxT("Bucharest, Romania"),44,25,'N',26,7,'E'},
	{wxT("Budapest, Hungary"),47,30,'N',19,5,'E'},
	{wxT("Buenos Aires, Argentina"),34,35,'S',58,22,'W'},
	{wxT("Buffalo, N.Y., USA"),42,55,'N',78,50,'W'},
	{wxT("Calgary, Alba., Canada"),51,1,'N',114,1,'W'},
	{wxT("Cairo, Egypt"),30,2,'N',31,21,'E'},
	{wxT("Calcutta, India"),22,34,'N',88,24,'E'},
	{wxT("Canton, China"),23,7,'N',113,15,'E'},
	{wxT("Cape Town, South Africa"),33,55,'S',18,22,'E'},
	{wxT("Caracas, Venezuela"),10,28,'N',67,2,'W'},
	{wxT("Carlsbad, N.M., USA"),32,26,'N',104,15,'W'},
	{wxT("Cayenne, French Guiana"),4,49,'N',52,18,'W'},
	{wxT("Charleston, S.C., USA"),32,47,'N',79,56,'W'},
	{wxT("Charleston, W. Va., USA"),38,21,'N',81,38,'W'},
	{wxT("Charlotte, N.C., USA"),35,14,'N',80,50,'W'},
	{wxT("Cheyenne, Wyo., USA"),41,9,'N',104,52,'W'},
	{wxT("Chicago, Ill., USA"),41,50,'N',87,37,'W'},
	{wxT("Chihuahua, Mexico"),28,37,'N',106,5,'W'},
	{wxT("Chongqing, China"),29,46,'N',106,34,'E'},
	{wxT("Cincinnati, Ohio, USA"),39,8,'N',84,30,'W'},
	{wxT("Cleveland, Ohio, USA"),41,28,'N',81,37,'W'},
	{wxT("Columbia, S.C., USA"),34,0,'N',81,2,'W'},
	{wxT("Columbus, Ohio, USA"),40,0,'N',83,1,'W'},
	{wxT("Copenhagen, Denmark"),55,40,'N',12,34,'E'},
	{wxT("Córdoba, Argentina"),31,28,'S',64,10,'W'},
	{wxT("Dakar, Senegal"),14,40,'N',17,28,'W'},
	{wxT("Dallas, Tex., USA"),32,46,'N',96,46,'W'},
	{wxT("Darwin, Australia"),12,28,'S',130,51,'E'},
	{wxT("Denver, Colo., USA"),39,45,'N',105,0,'W'},
	{wxT("Des Moines, Iowa, USA"),41,35,'N',93,37,'W'},
	{wxT("Detroit, Mich., USA"),42,20,'N',83,3,'W'},
	{wxT("Djibouti, Djibouti"),11,30,'N',43,3,'E'},
	{wxT("Dublin, Ireland"),53,20,'N',6,15,'W'},
	{wxT("Dubuque, Iowa, USA"),42,31,'N',90,40,'W'},
	{wxT("Duluth, Minn., USA"),46,49,'N',92,5,'W'},
	{wxT("Durban, South Africa"),29,53,'S',30,53,'E'},
	{wxT("Eastport, Maine, USA"),44,54,'N',67,0,'W'},
	{wxT("Edinburgh, Scotland"),55,55,'N',3,10,'W'},
	{wxT("Edmonton, Alb., Canada"),53,34,'N',113,28,'W'},
	{wxT("El Centro, Calif., USA"),32,38,'N',115,33,'W'},
	{wxT("El Paso, Tex., USA"),31,46,'N',106,29,'W'},
	{wxT("Eugene, Ore., USA"),44,3,'N',123,5,'W'},
	{wxT("Fargo, N.D., USA"),46,52,'N',96,48,'W'},
	{wxT("Flagstaff, Ariz., USA"),35,13,'N',111,41,'W'},
	{wxT("Fort Worth, Tex., USA"),32,43,'N',97,19,'W'},
	{wxT("Frankfurt, Germany"),50,7,'N',8,41,'E'},
	{wxT("Fresno, Calif., USA"),36,44,'N',119,48,'W'},
	{wxT("Georgetown, Guyana"),6,45,'N',58,15,'W'},
	{wxT("Glasgow, Scotland"),55,50,'N',4,15,'W'},
	{wxT("Grand Junction, Colo., USA"),39,5,'N',108,33,'W'},
	{wxT("Grand Rapids, Mich., USA"),42,58,'N',85,40,'W'},
	{wxT("Guatemala City, Guatemala"),14,37,'N',90,31,'W'},
	{wxT("Guayaquil, Ecuador"),2,10,'S',79,56,'W'},
	{wxT("Hamburg, Germany"),53,33,'N',10,2,'E'},
	{wxT("Hammerfest, Norway"),70,38,'N',23,38,'E'},
	{wxT("Havana, Cuba"),23,8,'N',82,23,'W'},
	{wxT("Havre, Mont., USA"),48,33,'N',109,43,'W'},
	{wxT("Helena, Mont., USA"),46,35,'N',112,2,'W'},
	{wxT("Helsinki, Finland"),60,10,'N',25,0,'E'},
	{wxT("Hobart, Tasmania"),42,52,'S',147,19,'E'},
	{wxT("Hong Kong, China"),2,2,' ',11,11,'E'},
	{wxT("Honolulu, Hawaii, USA"),21,18,'N',157,50,'W'},
	{wxT("Hot Springs, Ark., USA"),34,31,'N',93,3,'W'},
	{wxT("Houston, Tex., USA"),29,45,'N',95,21,'W'},
	{wxT("Idaho Falls, Idaho, USA"),43,30,'N',112,1,'W'},
	{wxT("Indianapolis, Ind., USA"),39,46,'N',86,10,'W'},
	{wxT("Iquique, Chile"),20,10,'S',70,7,'W'},
	{wxT("Irkutsk, Russia"),52,30,'N',104,20,'E'},
	{wxT("Jackson, Miss., USA"),32,20,'N',90,12,'W'},
	{wxT("Jacksonville, Fla., USA"),30,22,'N',81,40,'W'},
	{wxT("Jakarta, Indonesia"),6,16,'S',106,48,'E'},
	{wxT("Johannesburg, South Africa"),26,12,'S',28,4,'E'},
	{wxT("Juneau, Alaska, USA"),58,18,'N',134,24,'W'},
	{wxT("Kansas City, Mo., USA"),39,6,'N',94,35,'W'},
	{wxT("Key West, Fla., USA"),24,33,'N',81,48,'W'},
	{wxT("Kingston, Jamaica"),17,59,'N',76,49,'W'},
	{wxT("Kingston, Ont., Canada"),44,15,'N',76,30,'W'},
	{wxT("Kinshasa, Congo"),4,18,'S',15,17,'E'},
	{wxT("Klamath Falls, Ore., USA"),42,10,'N',121,44,'W'},
	{wxT("Knoxville, Tenn., USA"),35,57,'N',83,56,'W'},
	{wxT("Kuala Lumpur, Malaysia"),3,8,'N',101,42,'E'},
	{wxT("La Paz, Bolivia"),16,27,'S',68,22,'W'},
	{wxT("Las Vegas, Nev., USA"),36,10,'N',115,12,'W'},
	{wxT("Leeds, England"),53,45,'N',1,30,'W'},
	{wxT("Lewiston, Idaho, USA"),46,24,'N',117,2,'W'},
	{wxT("Lima, Peru"),12,0,'S',77,2,'W'},
	{wxT("Lincoln, Neb., USA"),40,50,'N',96,40,'W'},
	{wxT("Lisbon, Portugal"),38,44,'N',9,9,'W'},
	{wxT("Liverpool, England"),53,25,'N',3,0,'W'},
	{wxT("London, England"),51,32,'N',0,5,'W'},
	{wxT("London, Ont., Canada"),43,2,'N',81,34,'W'},
	{wxT("Long Beach, Calif., USA"),33,46,'N',118,11,'W'},
	{wxT("Los Angeles, Calif., USA"),34,3,'N',118,15,'W'},
	{wxT("Louisville, Ky., USA"),38,15,'N',85,46,'W'},
	{wxT("Lyons, France"),45,45,'N',4,50,'E'},
	{wxT("Madrid, Spain"),40,26,'N',3,42,'W'},
	{wxT("Manchester, England"),53,30,'N',2,15,'W'},
	{wxT("Manchester, N.H., USA"),43,0,'N',71,30,'W'},
	{wxT("Manila, Philippines"),14,35,'N',120,57,'E'},
	{wxT("Marseilles, France"),43,20,'N',5,20,'E'},
	{wxT("Mazatlán, Mexico"),23,12,'N',106,25,'W'},
	{wxT("Mecca, Saudi Arabia"),21,29,'N',39,45,'E'},
	{wxT("Melbourne, Australia"),37,47,'S',144,58,'E'},
	{wxT("Memphis, Tenn., USA"),35,9,'N',90,3,'W'},
	{wxT("Mexico City, Mexico"),19,26,'N',99,7,'W'},
	{wxT("Miami, Fla., USA"),25,46,'N',80,12,'W'},
	{wxT("Milan, Italy"),45,27,'N',9,10,'E'},
	{wxT("Milwaukee, Wis., USA"),43,2,'N',87,55,'W'},
	{wxT("Minneapolis, Minn., USA"),44,59,'N',93,14,'W'},
	{wxT("Mobile, Ala., USA"),30,42,'N',88,3,'W'},
	{wxT("Montevideo, Uruguay"),34,53,'S',56,10,'W'},
	{wxT("Montgomery, Ala., USA"),32,21,'N',86,18,'W'},
	{wxT("Montpelier, Vt., USA"),44,15,'N',72,32,'W'},
	{wxT("Montreal, Que., Canada"),45,30,'N',73,35,'W'},
	{wxT("Moose Jaw, Sask., Canada"),50,37,'N',105,31,'W'},
	{wxT("Moscow, Russia"),55,45,'N',37,36,'E'},
	{wxT("Munich, Germany"),48,8,'N',11,35,'E'},
	{wxT("Nagasaki, Japan"),32,48,'N',129,57,'E'},
	{wxT("Nagoya, Japan"),35,7,'N',136,56,'E'},
	{wxT("Nairobi, Kenya"),1,25,'S',36,55,'E'},
	{wxT("Nanjing (Nanking), China"),32,3,'N',118,53,'E'},
	{wxT("Naples, Italy"),40,50,'N',14,15,'E'},
	{wxT("Nashville, Tenn., USA"),36,10,'N',86,47,'W'},
	{wxT("Nelson, B.C., Canada"),49,30,'N',117,17,'W'},
	{wxT("New Delhi, India"),2,3,' ',7,12,'E'},
	{wxT("New Haven, Conn., USA"),41,19,'N',72,55,'W'},
	{wxT("New Orleans, La., USA"),29,57,'N',90,4,'W'},
	{wxT("New York, N.Y., USA"),40,47,'N',73,58,'W'},
	{wxT("Newark, N.J., USA"),40,44,'N',74,10,'W'},
	{wxT("Newcastle-on-Tyne, England"),54,58,'N',1,37,'W'},
	{wxT("Nome, Alaska, USA"),64,25,'N',165,30,'W'},
	{wxT("Oakland, Calif., USA"),37,48,'N',122,16,'W'},
	{wxT("Odessa, Ukraine"),46,27,'N',30,48,'E'},
	{wxT("Oklahoma City, Okla., USA"),35,26,'N',97,28,'W'},
	{wxT("Omaha, Neb., USA"),41,15,'N',95,56,'W'},
	{wxT("Osaka, Japan"),34,32,'N',135,30,'E'},
	{wxT("Oslo, Norway"),59,57,'N',10,42,'E'},
	{wxT("Ottawa, Ont., Canada"),45,24,'N',75,43,'W'},
	{wxT("Panama City, Panama"),8,58,'N',79,32,'W'},
	{wxT("Paramaribo, Suriname"),5,45,'N',55,15,'W'},
	{wxT("Paris, France"),48,48,'N',2,20,'E'},
	{wxT("Perth, Australia"),31,57,'S',115,52,'E'},
	{wxT("Philadelphia, Pa., USA"),39,57,'N',75,10,'W'},
	{wxT("Phoenix, Ariz., USA"),33,29,'N',112,4,'W'},
	{wxT("Pierre, S.D., USA"),44,22,'N',100,21,'W'},
	{wxT("Pittsburgh, Pa., USA"),40,27,'N',79,57,'W'},
	{wxT("Plymouth, England"),50,25,'N',4,5,'W'},
	{wxT("Port Moresby, Papua New Guinea"),9,25,'S',147,8,'E'},
	{wxT("Portland, Maine, USA"),43,40,'N',70,15,'W'},
	{wxT("Portland, Ore., USA"),45,31,'N',122,41,'W'},
	{wxT("Prague, Czech Republic"),50,5,'N',14,26,'E'},
	{wxT("Providence, R.I., USA"),41,50,'N',71,24,'W'},
	{wxT("Quebec, Que., Canada"),46,49,'N',71,11,'W'},
	{wxT("Raleigh, N.C., USA"),35,46,'N',78,39,'W'},
	{wxT("Rangoon, Myanmar"),16,50,'N',96,0,'E'},
	{wxT("Reno, Nev., USA"),39,30,'N',119,49,'W'},
	{wxT("Reykjavík, Iceland"),64,4,'N',21,58,'W'},
	{wxT("Richfield, Utah, USA"),38,46,'N',112,5,'W'},
	{wxT("Richmond, Va., USA"),37,33,'N',77,29,'W'},
	{wxT("Rio de Janeiro, Brazil"),22,57,'S',43,12,'W'},
	{wxT("Roanoke, Va., USA"),37,17,'N',79,57,'W'},
	{wxT("Rome, Italy"),41,54,'N',12,27,'E'},
	{wxT("Sacramento, Calif., USA"),38,35,'N',121,30,'W'},
	{wxT("Salt Lake City, Utah, USA"),40,46,'N',111,54,'W'},
	{wxT("Salvador, Brazil"),12,56,'S',38,27,'W'},
	{wxT("San Antonio, Tex., USA"),29,23,'N',98,33,'W'},
	{wxT("San Diego, Calif., USA"),32,42,'N',117,10,'W'},
	{wxT("San Francisco, Calif., USA"),37,47,'N',122,26,'W'},
	{wxT("San Jose, Calif., USA"),37,20,'N',121,53,'W'},
	{wxT("San Juan, P.R., USA"),18,30,'N',66,10,'W'},
	{wxT("Santa Fe, N.M., USA"),35,41,'N',105,57,'W'},
	{wxT("Santiago, Chile"),33,28,'S',70,45,'W'},
	{wxT("São Paulo, Brazil"),23,31,'S',46,31,'W'},
	{wxT("Savannah, Ga., USA"),32,5,'N',81,5,'W'},
	{wxT("Seattle, Wash., USA"),47,37,'N',122,20,'W'},
	{wxT("Shanghai, China"),31,10,'N',121,28,'E'},
	{wxT("Shreveport, La., USA"),32,28,'N',93,42,'W'},
	{wxT("Singapore, Singapore"),1,14,'N',103,55,'E'},
	{wxT("Sioux Falls, S.D., USA"),43,33,'N',96,44,'W'},
	{wxT("Sitka, Alaska, USA"),57,10,'N',135,15,'W'},
	{wxT("Sofia, Bulgaria"),42,40,'N',23,20,'E'},
	{wxT("Spokane, Wash., USA"),47,40,'N',117,26,'W'},
	{wxT("Springfield, Ill., USA"),39,48,'N',89,38,'W'},
	{wxT("Springfield, Mass., USA"),42,6,'N',72,34,'W'},
	{wxT("Springfield, Mo., USA"),37,13,'N',93,17,'W'},
	{wxT("St. John, N.B., Canada"),45,18,'N',66,10,'W'},
	{wxT("St. Louis, Mo., USA"),38,35,'N',90,12,'W'},
	{wxT("St. Petersburg, Russia"),59,56,'N',30,18,'E'},
	{wxT("Stockholm, Sweden"),59,17,'N',18,3,'E'},
	{wxT("Sydney, Australia"),34,0,'S',151,0,'E'},
	{wxT("Syracuse, N.Y., USA"),43,2,'N',76,8,'W'},
	{wxT("Tampa, Fla., USA"),27,57,'N',82,27,'W'},
	{wxT("Tananarive, Madagascar"),18,50,'S',47,33,'E'},
	{wxT("Teheran, Iran"),35,45,'N',51,45,'E'},
	{wxT("Tokyo, Japan"),35,40,'N',139,45,'E'},
	{wxT("Toledo, Ohio, USA"),41,39,'N',83,33,'W'},
	{wxT("Toronto, Ont., Canada"),43,40,'N',79,24,'W'},
	{wxT("Tripoli, Libya"),32,57,'N',13,12,'E'},
	{wxT("Tulsa, Okla., USA"),36,9,'N',95,59,'W'},
	{wxT("Venice, Italy"),45,26,'N',12,20,'E'},
	{wxT("Vancouver, B.C., Canada"),49,13,'N',123,6,'W'},
	{wxT("Veracruz, Mexico"),19,10,'N',96,10,'W'},
	{wxT("Victoria, B.C., Canada"),48,25,'N',123,21,'W'},
	{wxT("Vienna, Austria"),48,14,'N',16,20,'E'},
	{wxT("Virginia Beach, Va., USA"),36,51,'N',75,58,'W'},
	{wxT("Vladivostok, Russia"),43,10,'N',132,0,'E'},
	{wxT("Warsaw, Poland"),52,14,'N',21,0,'E'},
	{wxT("Washington, D.C., USA"),38,53,'N',77,2,'W'},
	{wxT("Wellington, New Zealand"),41,17,'S',174,47,'E'},
	{wxT("Wichita, Kan., USA"),37,43,'N',97,17,'W'},
	{wxT("Wilmington, N.C., USA"),34,14,'N',77,57,'W'},
	{wxT("Winnipeg, Man., Canada"),49,54,'N',97,7,'W'},
	{wxT("Zürich, Switzerland"),47,21,'N',8,31,'E'},
	{NULL,0,0,'N',0,0,'W'}
};

static unsigned findCity(RRVec2 latitudeLongitude)
{
	unsigned numCities = sizeof(cityLocations)/sizeof(cityLocations[0]);
	for (unsigned i=0;i<numCities;i++)
		if ((cityLocations[i].getLatitudeLongitude()-latitudeLongitude).length()<0.2f)
			return i;
	return 0;
}

WX_PG_IMPLEMENT_PROPERTY_CLASS(LocationProperty,wxPGProperty,RRVec2,const RRVec2&,TextCtrl)

LocationProperty::LocationProperty(const wxString& label, const wxString& help, int precision, const RRVec2& latitudeLongitude)
	: wxPGProperty(label,wxPG_LABEL)
{
	SetValue(WXVARIANT(latitudeLongitude));
	SetHelpString(help);

	unsigned numCities = sizeof(cityLocations)/sizeof(cityLocations[0]);
	cityStrings = new const wxChar*[numCities];
	cityValues = new long[numCities];
	for (unsigned i=0;i<numCities;i++)
	{
		cityStrings[i] = cityLocations[i].city;
		cityValues[i] = i;
	}
	AddPrivateChild(new wxEnumProperty(_("city"),wxPG_LABEL,cityStrings,cityValues,findCity(latitudeLongitude)));
	AddPrivateChild(new FloatProperty(_("latitude")+L" (\u00b0)",_("Location latitude, 0 for equator, -90 for south pole, 90 for north pole."),latitudeLongitude[0],precision,-90,90,1,false));
	AddPrivateChild(new FloatProperty(_("longitude")+L" (\u00b0)",_("Location longitude, -180..180, positive eastward and negative westward of Greenwich, London."),latitudeLongitude[1],precision,-180,180,1,true));
}

LocationProperty::~LocationProperty()
{
	delete[] cityValues;
	delete[] cityStrings;
}

void LocationProperty::RefreshChildren()
{
	if ( !GetChildCount() ) return;
	const RRVec2& latitudeLongitude = RRVec2RefFromVariant(m_value);
	Item(0)->SetValue((int)findCity(latitudeLongitude));
	Item(1)->SetValue(latitudeLongitude[0]);
	Item(2)->SetValue(latitudeLongitude[1]);
}

wxVariant LocationProperty::ChildChanged(wxVariant& thisValue, int childIndex, wxVariant& childValue) const
{
	RRVec2 latitudeLongitude;
	latitudeLongitude << thisValue;
	switch ( childIndex )
	{
		case 0:
			latitudeLongitude = cityLocations[childValue.GetInteger()].getLatitudeLongitude();
			break;
		case 1: 
			latitudeLongitude[0] = childValue.GetDouble();
			break;
		case 2: 
			latitudeLongitude[1] = childValue.GetDouble();
			break;
	}
	thisValue << latitudeLongitude;
	return thisValue;
}

#endif // SUPPORT_SCENEVIEWER
