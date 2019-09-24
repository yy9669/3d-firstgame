#include "ObserveMode.hpp"
#include "DrawLines.hpp"
#include "LitColorTextureProgram.hpp"
#include "Mesh.hpp"
#include "Sprite.hpp"
#include "DrawSprites.hpp"
#include "data_path.hpp"
#include "Sound.hpp"

#include <iostream>

Load< Sound::Sample > noise(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("cold-dunes.opus"));
});

Load< SpriteAtlas > trade_font_atlas(LoadTagDefault, []() -> SpriteAtlas const * {
	return new SpriteAtlas(data_path("trade-font"));
});

GLuint meshes_for_lit_color_texture_program = 0;
static Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer *ret = new MeshBuffer(data_path("city.pnct"));
	meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

static Load< Scene > scene(LoadTagLate, []() -> Scene const * {
	Scene *ret = new Scene();
	ret->load(data_path("city.scene"), [](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		auto &mesh = meshes->lookup(mesh_name);
		scene.drawables.emplace_back(transform);
		Scene::Drawable::Pipeline &pipeline = scene.drawables.back().pipeline;

		pipeline = lit_color_texture_program_pipeline;
		pipeline.vao = meshes_for_lit_color_texture_program;
		pipeline.type = mesh.type;
		pipeline.start = mesh.start;
		pipeline.count = mesh.count;
	});
	return ret;
});

ObserveMode::ObserveMode() {
	assert(scene->cameras.size() && "Observe requires cameras.");

	current_camera = &scene->cameras.front();

	noise_loop = Sound::loop_3D(*noise, 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 10.0f);
}

ObserveMode::~ObserveMode() {
	noise_loop->stop();
}

bool ObserveMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		judge=false;
		if (evt.key.keysym.sym == SDLK_LEFT) {
			auto ci = scene->cameras.begin();
			while (ci != scene->cameras.end() && &*ci != current_camera) ++ci;
			if (ci == scene->cameras.begin()) ci = scene->cameras.end();
			--ci;
			current_camera = &*ci;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			auto ci = scene->cameras.begin();
			while (ci != scene->cameras.end() && &*ci != current_camera) ++ci;
			if (ci != scene->cameras.end()) ++ci;
			if (ci == scene->cameras.end()) ci = scene->cameras.begin();
			current_camera = &*ci;

			return true;
		}else if (evt.key.keysym.sym == SDLK_UP) {
			grass_num+=1;
			if(grass_num>5){
				grass_num=5;
			}
		}else if (evt.key.keysym.sym == SDLK_DOWN) {
			grass_num-=1;
			if(grass_num<0){
				grass_num=0;
			}
		}
	}else if (evt.button.button == SDL_BUTTON_LEFT){
		judge=true;
	}
	
	return false;
}

void ObserveMode::update(float elapsed) {
	noise_angle = std::fmod(noise_angle + elapsed, 2.0f * 3.1415926f);

	//update sound position:
	glm::vec3 center = glm::vec3(10.0f, 4.0f, 1.0f);
	float radius = 10.0f;
	noise_loop->set_position(center + radius * glm::vec3( std::cos(noise_angle), std::sin(noise_angle), 0.0f));

	//update listener position:
	glm::mat4 frame = current_camera->transform->make_local_to_world();

	//using the sound lock here because I want to update position and right-direction *simultaneously* for the audio code:
	Sound::lock();
	Sound::listener.set_position(frame[3]);
	Sound::listener.set_right(frame[0]);
	Sound::unlock();
}

void ObserveMode::draw(glm::uvec2 const &drawable_size) {
	//--- actual drawing ---
	glClearColor(0.85f, 0.85f, 0.90f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	const_cast< Scene::Camera * >(current_camera)->aspect = drawable_size.x / float(drawable_size.y);

	scene->draw(*current_camera);

	{ //help text overlay:
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		DrawSprites draw(*trade_font_atlas, glm::vec2(0,0), glm::vec2(320, 200), drawable_size, DrawSprites::AlignPixelPerfect);

		std::string help_text = "--- Use Left/Right to switch cameras ---";
		glm::vec2 min, max;
		draw.get_text_extents(help_text, glm::vec2(0.0f, 0.0f), 1.0f, &min, &max);
		float x = std::round(160.0f - (0.5f * (max.x + min.x)));
		draw.draw_text(help_text, glm::vec2(x, 2.0f), 1.0f, glm::u8vec4(0xff,0xff,0xff,0xff));

		std::string help_text1 = "--- Use Left click to confirm the number ---";
		glm::vec2 min1, max1;
		draw.get_text_extents(help_text, glm::vec2(0.0f, 0.0f), 1.0f, &min1, &max1);
		float x1 = std::round(160.0f - (0.5f * (max1.x + min1.x)));
		draw.draw_text(help_text1, glm::vec2(x1, -30.0f), 1.0f, glm::u8vec4(0xff,0xff,0xff,0xff));

		std::string help_text2 = "--- Use Up/Down to change grass num ---";
		glm::vec2 min2, max2;
		draw.get_text_extents(help_text2, glm::vec2(0.0f, 0.0f), 1.0f, &min2, &max2);
		float x2 = std::round(160.0f - (0.5f * (max2.x + min2.x)));
		draw.draw_text(help_text2, glm::vec2(x2, 30.0f), 1.0f, glm::u8vec4(0xff,0xff,0xff,0xff));

		if(grass_num<=0){
			std::string gnum = "0";
			glm::vec2 gmin,gmax;
			draw.get_text_extents(gnum, glm::vec2(0.0f, 0.0f), 1.0f, &gmin, &gmax);
			float gx = std::round(160.0f - (0.5f * (gmax.x + gmin.x)));
			draw.draw_text(gnum, glm::vec2(gx, 130.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
		}else if(grass_num==1){
			std::string gnum = "1";
			glm::vec2 gmin,gmax;
			draw.get_text_extents(gnum, glm::vec2(0.0f, 0.0f), 1.0f, &gmin, &gmax);
			float gx = std::round(160.0f - (0.5f * (gmax.x + gmin.x)));
			draw.draw_text(gnum, glm::vec2(gx, 130.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
		}else if(grass_num==2){
			std::string gnum = "2";
			glm::vec2 gmin,gmax;
			draw.get_text_extents(gnum, glm::vec2(0.0f, 0.0f), 1.0f, &gmin, &gmax);
			float gx = std::round(160.0f - (0.5f * (gmax.x + gmin.x)));
			draw.draw_text(gnum, glm::vec2(gx, 130.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
		}else if(grass_num==3){
			std::string gnum = "3";
			glm::vec2 gmin,gmax;
			draw.get_text_extents(gnum, glm::vec2(0.0f, 0.0f), 1.0f, &gmin, &gmax);
			float gx = std::round(160.0f - (0.5f * (gmax.x + gmin.x)));
			draw.draw_text(gnum, glm::vec2(gx, 130.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
		}else if(grass_num==4){
			std::string gnum = "4";
			glm::vec2 gmin,gmax;
			draw.get_text_extents(gnum, glm::vec2(0.0f, 0.0f), 1.0f, &gmin, &gmax);
			float gx = std::round(160.0f - (0.5f * (gmax.x + gmin.x)));
			draw.draw_text(gnum, glm::vec2(gx, 130.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
		}else{
			std::string gnum = "5";
			glm::vec2 gmin,gmax;
			draw.get_text_extents(gnum, glm::vec2(0.0f, 0.0f), 1.0f, &gmin, &gmax);
			float gx = std::round(160.0f - (0.5f * (gmax.x + gmin.x)));
			draw.draw_text(gnum, glm::vec2(gx, 130.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
		}
		if(judge==true){
			if(grass_num==3){
				std::string cnum = "You are right";
				glm::vec2 cmin,cmax;
				draw.get_text_extents(cnum, glm::vec2(0.0f, 0.0f), 1.0f, &cmin, &cmax);
				float cx = std::round(160.0f - (0.5f * (cmax.x + cmin.x)));
				draw.draw_text(cnum, glm::vec2(cx, 100.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));	
			}else{
				std::string cnum = "You are wrong";
				glm::vec2 cmin,cmax;
				draw.get_text_extents(cnum, glm::vec2(0.0f, 0.0f), 1.0f, &cmin, &cmax);
				float cx = std::round(160.0f - (0.5f * (cmax.x + cmin.x)));
				draw.draw_text(cnum, glm::vec2(cx, 100.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));	
			}
		}



	}
}
