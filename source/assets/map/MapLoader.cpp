#include "MapLoader.hpp"

#include <tinyxml.h>
#include <iostream>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include <assets/map/XmlHelper.hpp>
#include <engine/env/Environment.hpp>
#include <engine/BoxCollider.hpp>
#include <engine/Light.hpp>
#include <engine/core/math/Vector3f.hpp>

#include <engine/component/Transform.hpp>
#include <engine/component/MeshDrawable.hpp>
#include <engine/component/AACollisionBox.hpp>
#include <engine/component/Trigger.hpp>
#include <engine/component/Health.hpp>
#include "../../PlayerMotion.hpp"

#include <assets/scene/Scene.hpp>
#include <assets/model/Mesh.hpp>
#include <assets/texture/Texture.hpp>
#include <assets/gui/GUIButton.hpp>

#include <assets/model/MeshLoader.hpp>
#include <assets/texture/TextureLoader.hpp>
#include <assets/material/MaterialLoader.hpp>

using namespace std;

namespace glPortal {
/** \class MapLoader
 *  Load a map in GlPortal XML format.
 */

  Scene* MapLoader::scene;
  TiXmlHandle MapLoader::rootHandle = TiXmlHandle(0);

/**
 * Get a scene from a map file in XML format.
 */
Scene* MapLoader::getScene(const std::string &path) {
  scene = new Scene();
  scene->player.addComponent<Transform>();
  scene->player.addComponent<PlayerMotion>();
  scene->player.addComponent<Health>();

  TiXmlDocument doc(Environment::getDataDir() + "/maps/" + path + ".xml");
  bool loaded = doc.LoadFile();

  if (loaded) {
    TiXmlHandle docHandle(&doc);
    TiXmlElement *element = docHandle.FirstChildElement().ToElement();
    rootHandle = TiXmlHandle(element);

    extractMaterials();
    extractSpawn();
    extractDoor();
    extractModels();
    extractLights();
    extractWalls();
    extractAcids();
    extractTriggers();
    cout << "File loaded." << endl;
  } else {
    cout << "Unable to load file. " << endl;
    cout << Environment::getDataDir() << "/" << path << ".xml" << endl;
  }
  return scene;
}

void MapLoader::extractMaterials() {
  TiXmlElement *matIdxElm = rootHandle.FirstChild("materials").ToElement();

  if (matIdxElm) {
    TiXmlElement *matElm = matIdxElm->FirstChildElement("mat");
    if (matElm) {
      do {
        int mid = -1;
        matElm->QueryIntAttribute("mid", &mid);
        if (mid == -1) {
          continue; // Ignore, no good Material ID bound to it
        }
        std::string name("");
        matElm->QueryStringAttribute("name", &name);
        if (name.length() > 0) {
          scene->materials[mid] = MaterialLoader::getMaterial(name);
        } else {
          continue; // No name, no candy
        }
      } while ((matElm = matElm->NextSiblingElement("mat")) != nullptr);
    }
  }
}

/**
 * Extract a spawn element containing its rotation and position elements
 */
void MapLoader::extractSpawn() {
  TiXmlElement *spawnElement = rootHandle.FirstChild("spawn").ToElement();

  if (spawnElement) {
    scene->start.clearComponents();
    Transform &t = scene->start.addComponent<Transform>();
    XmlHelper::extractPosition(spawnElement, t.position);
    XmlHelper::extractRotation(spawnElement, t.rotation);
    Transform &pt = scene->player.getComponent<Transform>();
    pt.position = t.position;
    pt.rotation = t.rotation;
  } else {
    throw std::runtime_error("No spawn position defined.");
  }
}

/**
 * Extract a light elements containing position (x, y, z) and colour (r, g, b) attributes
 */
void MapLoader::extractLights() {
  Vector3f lightPos;
  Vector3f lightColor;
  float distance, energy, specular;
  TiXmlElement* lightElement = rootHandle.FirstChild("light").ToElement();

  do {
    XmlHelper::pushAttributeVertexToVector(lightElement, lightPos);

    lightElement->QueryFloatAttribute("r", &lightColor.x);
    lightElement->QueryFloatAttribute("g", &lightColor.y);
    lightElement->QueryFloatAttribute("b", &lightColor.z);

    lightElement->QueryFloatAttribute("distance", &distance);
    lightElement->QueryFloatAttribute("energy", &energy);
    lightElement->QueryFloatAttribute("specular", &specular);

    scene->lights.emplace_back();
    Light &light = scene->lights.back();
    light.position.set(lightPos.x, lightPos.y, lightPos.z);
    light.color.set(lightColor.x, lightColor.y, lightColor.z);
    light.distance = distance;
    light.energy = energy;
    light.specular = specular;
  } while ((lightElement = lightElement->NextSiblingElement("light")) != nullptr);
}

void MapLoader::extractDoor() {
  TiXmlElement *endElement = rootHandle.FirstChild("end").ToElement();

  if (endElement) {
    Entity &door = scene->end;
    door.clearComponents();
    Transform &t = door.addComponent<Transform>();
    XmlHelper::extractPosition(endElement, t.position);
    XmlHelper::extractRotation(endElement, t.rotation);
    MeshDrawable &m = door.addComponent<MeshDrawable>();
    m.material = MaterialLoader::fromTexture("Door.png");
    m.mesh = MeshLoader::getMesh("Door.obj");
  }
}

void MapLoader::extractWalls() {
  TiXmlElement *wallBoxElement = rootHandle.FirstChildElement("wall").ToElement();

  if (wallBoxElement) {
    do {
      scene->entities.emplace_back();
      Entity &wall = scene->entities.back();

      Transform &t = wall.addComponent<Transform>();
      XmlHelper::extractPosition(wallBoxElement, t.position);
      XmlHelper::extractRotation(wallBoxElement, t.rotation);
      XmlHelper::extractScale(wallBoxElement, t.scale);

      int mid = -1;
      wallBoxElement->QueryIntAttribute("mid", &mid);
      MeshDrawable &m = wall.addComponent<MeshDrawable>();
      m.material = scene->materials[mid];
      m.material.scaleU = m.material.scaleV = 2.f;
      m.mesh = MeshLoader::getPortalBox(wall);
      wall.addComponent<AACollisionBox>().box = BoxCollider::generateCage(wall);
    } while ((wallBoxElement = wallBoxElement->NextSiblingElement("wall")) != nullptr);
  }
}

void MapLoader::extractAcids() {
  TiXmlElement *acidElement = rootHandle.FirstChild("acid").ToElement();
  
  if (acidElement) {
    do {
      scene->entities.emplace_back();
      Entity &acid = scene->entities.back();

      Transform &t = acid.addComponent<Transform>();
      XmlHelper::extractPosition(acidElement, t.position);
      XmlHelper::extractScale(acidElement, t.scale);

      MeshDrawable &m = acid.addComponent<MeshDrawable>();
      m.material.diffuse = TextureLoader::getTexture("acid.png");
      m.mesh = MeshLoader::getPortalBox(acid);
      acid.addComponent<AACollisionBox>().box = BoxCollider::generateCage(acid);
    } while ((acidElement = acidElement->NextSiblingElement("acid")) != nullptr);
  }
}

void MapLoader::extractTriggers() {
  TiXmlElement *triggerElement = rootHandle.FirstChild("trigger").ToElement();

  if (triggerElement) {
    do {
      scene->entities.emplace_back();
      Entity &trigger = scene->entities.back();

      Transform &t = trigger.addComponent<Transform>();
      XmlHelper::extractPosition(triggerElement, t.position);
      XmlHelper::extractScale(triggerElement, t.scale);
      
      Trigger &tgr = trigger.addComponent<Trigger>();
      triggerElement->QueryStringAttribute("type", &tgr.type);

    } while ((triggerElement = triggerElement->NextSiblingElement("trigger")) != nullptr);
  }
}

void MapLoader::extractModels() {
  Vector3f modelPos;
  string texture("none");
  string mesh("none");
  TiXmlElement *modelElement = rootHandle.FirstChild("model").ToElement();
  if (modelElement){
    do {
      modelElement->QueryStringAttribute("texture", &texture);
      modelElement->QueryStringAttribute("mesh", &mesh);
      XmlHelper::pushAttributeVertexToVector(modelElement, modelPos);

      scene->entities.emplace_back();
      Entity &model = scene->entities.back();
      Transform &t = model.addComponent<Transform>();
      XmlHelper::extractPosition(modelElement, t.position);
      XmlHelper::extractRotation(modelElement, t.rotation);
      MeshDrawable &m = model.addComponent<MeshDrawable>();
      m.material = MaterialLoader::fromTexture(texture);
      m.mesh = MeshLoader::getMesh(mesh);
    } while ((modelElement = modelElement->NextSiblingElement("model")) != nullptr);
  }
}

void MapLoader::extractButtons() {
  // FIXME
#if 0
  TiXmlElement *textureElement = rootHandle.FirstChild("texture").ToElement();
  string texturePath("none");
  string surfaceType("none");
  Vector2f position;
  Vector2f size;

  if (textureElement) {
    do {
      textureElement->QueryStringAttribute("source", &texturePath);
      textureElement->QueryStringAttribute("type", &surfaceType);
      TiXmlElement *buttonElement = textureElement->FirstChildElement("GUIbutton");

      if (buttonElement) {
        do {
          scene->buttons.emplace_back();
          GUIButton &button = scene->buttons.back();

          buttonElement->QueryFloatAttribute("x", &position.x);
          buttonElement->QueryFloatAttribute("y", &position.y);

          buttonElement->QueryFloatAttribute("w", &size.x);
          buttonElement->QueryFloatAttribute("h", &size.y);

          button.material = MaterialLoader::fromTexture(texturePath);
          button.material.scaleU = button.material.scaleV = 2.f;
        } while ((buttonElement = buttonElement->NextSiblingElement("GUIbutton")) != nullptr);
      }

      texturePath = "none";
    } while ((textureElement = textureElement->NextSiblingElement("texture")) != nullptr);
  }
#endif
}
} /* namespace glPortal */
