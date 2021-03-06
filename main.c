#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>


#include "sim.h"

#define pi 3.14159265358979323

#define WIDTH 50
#define HEIGHT 50
#define CELLSIZE 8
#define TICKTIME 0.2
#define TIMERINTERVAL 50
#define STEPSPERTICK 2

#define WINDOW_W 1400
#define WINDOW_H 960

#define DRAWFLOW 0

#define ISOWIDTH 16
#define ISOHEIGHT 8
#define HEIGHTMULT 3

#define MAXDROPPEDFRAMES 10

#define RAIN_MILLIS 100
#define RAIN_AMT 30
#define RADIAL_AMT 100
#define RADIAL_PERIOD 1950
#define LINEAR_AMT 30
#define LINEAR_PERIOD 1950

#define DEBUGPRINT 0

SDL_Renderer *setupWindow();
Uint32 timercallback(Uint32 interval, void *param);
void cleanup();
void drawGrid(SDL_Renderer *r, sim_Sim *s, SDL_Rect *viewport);
void drawIso(SDL_Renderer *r, sim_Sim *s, SDL_Rect *viewport);
void drawTools(SDL_Renderer *r, struct _uistate *ui, SDL_Rect *viewport);
void doMouseEvent(struct _uistate *ui, sim_Sim *s);
void doDemos(int current, sim_Sim *s);
void printGrid(sim_Sim *s);

void starttimer();
void stoptimer();

int SDL_PointInRect(SDL_Point *p, SDL_Rect *r) {
	return (int)(p->x >= r->x && p->y >= r->y && p->x <= r->x + r->w && p->y <= r->y + r->h);
}

struct timerparams {
	int CustomTimerEvent;
	int timerlock;
};




SDL_Texture *iconText;
SDL_Texture *isoText;
SDL_Rect gridviewport, isoviewport, toolbox;

enum {
	TOOL_ADD,
	TOOL_REMOVE
};

enum {
	MODE_NONE,
	MODE_LINEAR,
	MODE_RADIAL,
	MODE_RAIN
};

struct _uistate {
	int toolselected;
	int demoselected;
	char paused;
	int hoverx, hovery;
	char mouseinwindow;
}  uistate;

int main(int argc, char *argv[]){
	int i;
	struct timerparams tparams;
	SDL_Event e;
	sim_Sim *mysim;
	SDL_Renderer *ren;
	int temp;
	SDL_Surface *iconSurf, *isoSurf;
	SDL_TimerID timerid;
	Uint32 updatetime;
	int tickspersecond;
	int droppedframes;

	gridviewport.x = 0;
	gridviewport.y = 0;
	gridviewport.w = WIDTH * CELLSIZE;
	gridviewport.h = HEIGHT * CELLSIZE;

	isoviewport.x = 500;
	isoviewport.y = 700;

	toolbox.x = 0;
	toolbox.y = 400;
	toolbox.w = 400;
	toolbox.h = 400;

	uistate.toolselected = TOOL_ADD;
	uistate.demoselected = MODE_NONE;
	uistate.paused = 0;
	uistate.mouseinwindow = 1;

	ren = setupWindow();

	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

	if((iconSurf = SDL_LoadBMP("icons.bmp")) == NULL) {
		printf("Failed to load icons.bmp");
		exit(-1);
	}
	iconText = SDL_CreateTextureFromSurface(ren, iconSurf);

	isoSurf = SDL_LoadBMP("tile.bmp");
	isoText = SDL_CreateTextureFromSurface(ren, isoSurf);

	if((tparams.CustomTimerEvent = SDL_RegisterEvents(1)) == -1){
		printf("Failed to register customevent\n");
		exit(-1);
	}
	tparams.timerlock = 0;

	if((timerid = SDL_AddTimer(TIMERINTERVAL, timercallback, (void *) &tparams)))
		;
	else
		fprintf(stderr,"Timer failed to start\n");


	mysim = sim_CreateSimulation(WIDTH, HEIGHT, 50.5);

	int quit = 0;

	updatetime = SDL_GetTicks();
	tickspersecond = 0;
	

	while (!quit) {
		droppedframes = MAXDROPPEDFRAMES;

		doMouseEvent(&uistate, mysim);
			
		drawGrid(ren, mysim, &gridviewport);
		drawIso(ren, mysim, &isoviewport);
		drawTools(ren, &uistate, &toolbox);
		SDL_RenderPresent(ren);

		
		//fprintf(stderr, "b");
		if(SDL_GetTicks() - updatetime > 1000) {
			updatetime = SDL_GetTicks();
			if(DEBUGPRINT)
				fprintf(stderr, "%d\n", tickspersecond);
			tickspersecond = 0;
		}


		while ((temp = SDL_PollEvent(&e)) != 0 && !quit) {
			switch (e.type) {
				case SDL_QUIT:
					quit = 1;
					break;
				case SDL_USEREVENT:
					if(!droppedframes || uistate.paused)
						break; //If it drops too many frames, clear the event queue
					droppedframes--;
					//fprintf(stderr, "c");
					tickspersecond++;
					for(i = 0; i < STEPSPERTICK; i++) {
						doDemos(uistate.demoselected, mysim);
						sim_step(mysim, TICKTIME);
					}
					//printGrid(mysim);
					tparams.timerlock = 0;
					break;
				case SDL_MOUSEMOTION:
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
					//doMouseEvent(&e, &selected, &hovered, mysim);
					break;
				case SDL_WINDOWEVENT:
					switch (e.window.event) {
						case SDL_WINDOWEVENT_ENTER:
							uistate.mouseinwindow = 1;
						case SDL_WINDOWEVENT_LEAVE:
							uistate.mouseinwindow = 0;
							break;
					}
					break;
				default:
					if(DEBUGPRINT) 
						fprintf(stderr, "UNKNOWN EVENT TYPE\n");
					break;
			}
		}
	}

	cleanup();
	return 0;
}


void printGrid(sim_Sim *s) {
	int i, j;
	sim_Cell *cell;

	for(i = 0; i < s->h; i++) {
		for(j = 0; j < s->w; j++) {
			cell = &(cellAt(j,i,s));
			fprintf(stderr,"%4.0lf ", cell->height);
		}
		fprintf(stderr,"\n");
	}	
	fprintf(stderr, "\nAvgheight:%lf\n---\n\n", sim_avgheight(s));
	fprintf(stderr, "\n---\n\n");
}

void drawGrid(SDL_Renderer *r, sim_Sim *s, SDL_Rect *viewport) {
	int i, j;
	sim_Cell *cell;
	SDL_Rect rect;
	rect.w = CELLSIZE;
	rect.h = CELLSIZE;

	SDL_SetRenderDrawColor(r, 200, 255, 200, 255);
	SDL_RenderClear(r);


	for(i = 0; i < s->h; i++) {
		for(j = 0; j < s->w; j++) {
			cell = &(cellAt(j,i,s));

			int rg = 255 - (int)cell->height;
			rg = rg < 0 ? 0 : rg;
			rect.x = j * CELLSIZE + viewport->x;
			rect.y = i * CELLSIZE + viewport->y;

			SDL_SetRenderDrawColor(r, rg, rg, 255, 255); 
			SDL_RenderFillRect(r, &rect);

			SDL_SetRenderDrawColor(r, 0, 0, 0, 255);


		}
	}	
	if(DRAWFLOW)
		for(i = 0; i < s->h; i++) {
			for(j = 0; j < s->w; j++) {
				cell = &(cellAt(j,i,s));
				rect.x = j * CELLSIZE + viewport->x;
				rect.y = i * CELLSIZE + viewport->y;
				int len;
				len = cell->flow[RT];
				SDL_SetRenderDrawColor(r, len > 0 ? 0 : 255, 0, 0, 255); 
				len = abs(len);
				SDL_RenderDrawLine(r, 
					rect.x + CELLSIZE / 2,
					rect.y + CELLSIZE / 2,
					rect.x + CELLSIZE / 2 + len,
					rect.y + CELLSIZE / 2);
				len = cell->flow[LF];
				SDL_SetRenderDrawColor(r, len > 0 ? 0 : 255, 0, 0, 255); 
				len = abs(len);
				SDL_RenderDrawLine(r, 
					rect.x + CELLSIZE / 2,
					rect.y + CELLSIZE / 2,
					rect.x + CELLSIZE / 2 - len,
					rect.y + CELLSIZE / 2);
				len = cell->flow[DN];
				SDL_SetRenderDrawColor(r, len > 0 ? 0 : 255, 0, 0, 255); 
				len = abs(len);
				SDL_RenderDrawLine(r, 
					rect.x + CELLSIZE / 2,
					rect.y + CELLSIZE / 2,
					rect.x + CELLSIZE / 2,
					rect.y + CELLSIZE / 2 + len);
				len = cell->flow[UP];
				SDL_SetRenderDrawColor(r, len > 0 ? 0 : 255, 0, 0, 255); 
				len = abs(len);
				SDL_RenderDrawLine(r, 
					rect.x + CELLSIZE / 2,
					rect.y + CELLSIZE / 2,
					rect.x + CELLSIZE / 2,
					rect.y + CELLSIZE / 2 - len);
			}
		}
}

void drawIso(SDL_Renderer *r, sim_Sim *s, SDL_Rect *viewport) {
	int i, j;
	sim_Cell *cell;
	SDL_Rect srcrect, dstrect,vertrect;

	srcrect.x = 0;
	srcrect.y = 0;
	srcrect.w = ISOWIDTH;
	srcrect.h = ISOHEIGHT;

	dstrect.w = ISOWIDTH;
	dstrect.h = ISOHEIGHT;

	vertrect.w = ISOWIDTH / 2;


	for(i = 0; i < s->h; i++) {
		for(j = s->w - 1; j >= 0; j--) {
			cell = &(cellAt(j,i,s));

			dstrect.x = j * ISOWIDTH / 2 + i * ISOWIDTH / 2 + viewport->x;
			dstrect.y = i * ISOHEIGHT / 2 - j * ISOHEIGHT / 2 + viewport->y;

			vertrect.x = dstrect.x;
			vertrect.y = dstrect.y + ISOHEIGHT / 2;
			vertrect.h = cell->height * HEIGHTMULT;
			vertrect.y -= vertrect.h;

			dstrect.y -= cell->height * HEIGHTMULT;


			SDL_SetRenderDrawColor(r, 0, 0, 100, 255);
			SDL_RenderFillRect(r, &vertrect);

			vertrect.x += ISOWIDTH / 2;
			SDL_SetRenderDrawColor(r, 0, 0, 150, 255);
			SDL_RenderFillRect(r, &vertrect);


			SDL_RenderCopy(r, isoText, &srcrect, &dstrect);
		}
	}	
}

void drawTools(SDL_Renderer *r, struct _uistate *ui, SDL_Rect *viewport) {
	SDL_Rect temp;
	temp.x = 0;//copy from origin on src texture
	temp.y = 0;
	temp.w = viewport->w;
	temp.h = viewport->h;
	SDL_RenderCopy(r, iconText, &temp, viewport);

	if(ui->hoverx != -1) {//Draw hover rectangle
		temp.x = viewport->x;
		temp.y = viewport->y;
		temp.w = 100;
		temp.h = 100;

		temp.y += 100 * ui->hovery;
		temp.x += 100 * ui->hoverx;

		SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
		SDL_RenderDrawRect(r, &temp);
	}

	temp.w = 100;
	temp.h = 100;

	temp.x = viewport->x;
	temp.y = viewport->y + 100 * ui->toolselected;
	SDL_SetRenderDrawColor(r, 0, 0, 255, 100);
	SDL_RenderFillRect(r, &temp);


	temp.x = viewport->x + 200;
	temp.y = viewport->y + 100 * ui->demoselected;
	SDL_SetRenderDrawColor(r, 0, 0, 255, 100);
	SDL_RenderFillRect(r, &temp);

	if(ui->paused) {
		temp.x = viewport->x + 300;
		temp.y = viewport->y;
		SDL_SetRenderDrawColor(r, 0, 0, 255, 100);
		SDL_RenderFillRect(r, &temp);
	}
}

void doMouseEvent(struct _uistate *ui, sim_Sim *s) {
	static int laststate, clicked;
	Uint32 buttons;
	SDL_Point loc;
	
	buttons = SDL_GetMouseState(&(loc.x), &(loc.y));
	clicked = laststate & SDL_BUTTON(SDL_BUTTON_LEFT) && !(buttons & SDL_BUTTON(SDL_BUTTON_LEFT));
	buttons &= SDL_BUTTON(SDL_BUTTON_LEFT);

	ui->hoverx = -1;

	if(SDL_PointInRect(&loc, &toolbox)) {//Handle clicks in the ui
		ui->hovery = (loc.y - toolbox.y) / 100;
		ui->hoverx = (loc.x - toolbox.x) / 100;
		switch(ui->hoverx) {
			case 0://Tools
				if(buttons)
					ui->toolselected = ui->hovery;
				break;
			case 1://None
				break;
			case 2://demo
				if(buttons)
					ui->demoselected = ui->hovery;
				break;
			case 3://Pause
				if(clicked && ui->hovery == 0)
					ui->paused = !ui->paused;
				break;
		}
		//Each button in the toolbox is 100px high
	} else if(SDL_PointInRect(&loc, &gridviewport)) {//Handle clicks in the wave grid
		switch(ui->toolselected) {
			case TOOL_ADD: //Add water
				if(!clicked)
					break;
				loc.x -= gridviewport.x;
				loc.y -= gridviewport.y;
				loc.x /= CELLSIZE;
				loc.y /= CELLSIZE;
				cellAt(loc.x, loc.y, s).height += 200;
				//cellAt(loc.x - 1, loc.y, s).height += 200;
				//cellAt(loc.x + 1, loc.y, s).height += 200;
				//cellAt(loc.x, loc.y - 1, s).height += 200;
				//cellAt(loc.x, loc.y + 1, s).height += 200;
				break;
			case TOOL_REMOVE:
				if(!clicked)
					break;
				loc.x -= gridviewport.x;
				loc.y -= gridviewport.y;
				loc.x /= CELLSIZE;
				loc.y /= CELLSIZE;
				cellAt(loc.x, loc.y, s).height = 0;
				//Remove water

				break;
		}
	} else {//???
		
	}

	laststate = buttons;

}


void doDemos(int current, sim_Sim *s) {
	//Handle demos
	switch(current) {
		case MODE_NONE:
			break;
		case MODE_LINEAR://NOT IMPLEMENTED
			static int lineartimer = 0;
			int i;
			lineartimer += TIMERINTERVAL;
			while(lineartimer > LINEAR_PERIOD) {
				lineartimer -= LINEAR_PERIOD;	
				//Run once ever RAIN_MILLIS milliseconds
				for(i = 0; i < WIDTH; i++)
					cellAt(i, 0, s).height += LINEAR_AMT;
			}
			break;
		case MODE_RADIAL://NOT IMPLEMENTED
			static int radialtimer = 0;
			radialtimer += TIMERINTERVAL;
			while(radialtimer > RADIAL_PERIOD) {
				radialtimer -= RADIAL_PERIOD;	
				//Run once ever RAIN_MILLIS milliseconds
				cellAt(WIDTH / 2, HEIGHT / 2, s).height += RADIAL_AMT;
			}
			break;
		case MODE_RAIN:
			static int raintimer = 0;
			raintimer += TIMERINTERVAL;
			while(raintimer > RAIN_MILLIS) {
				raintimer -= RAIN_MILLIS;	
				//Run once ever RAIN_MILLIS milliseconds
				cellAt(rand() % WIDTH, rand() % HEIGHT, s).height += RAIN_AMT;
			}
			break;
	}
}

void cleanup() {
	SDL_Quit();
}

Uint32 timercallback(Uint32 interval, void *param) {
	struct timerparams *tparams = (struct timerparams *) param;
	SDL_Event event;
	SDL_zero(event);
	event.type = tparams->CustomTimerEvent;

	//if(!tparams->timerlock){
		SDL_PushEvent(&event);
		tparams->timerlock = 1;
	//}

	return interval;
}

SDL_Renderer *setupWindow() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
		printf("SDL_Init Error: %s",SDL_GetError());
		exit(-1);
	}

	SDL_Window *win = SDL_CreateWindow(
		"Wave Machine", 
		SDL_WINDOWPOS_CENTERED, 
		100, 
		WINDOW_W, 
		WINDOW_H, 
		SDL_WINDOW_SHOWN);
	if (win == NULL){
		printf("SDL_CreateWindow Error: %s",SDL_GetError());
		SDL_Quit();
		exit(-1);
	}

	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == NULL){
		SDL_DestroyWindow(win);
		printf("SDL_CreateRenderer Error: %s",SDL_GetError());
		SDL_Quit();
		exit(-1);
	}

	return ren;
}
