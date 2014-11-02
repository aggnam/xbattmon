char *colors[] = {
	[COLOR_BAT_CHARGED]     = "green",
	[COLOR_BAT_LEFT2CHARGE] = "grey",
	[COLOR_BAT_DRAINED]     = "red",
	[COLOR_BAT_LEFT2DRAIN]  = "blue"
};

unsigned int thickness = 2;	/* 2 pixels by default */
time_t pollinterval = 5;	/* poll battery state every 5 seconds */
int bottom = 1;			/* set to 0 if you want the bar to be at the top */
int maxcap = 100;		/* maximum battery capacity */
int raise = 0;			/* set to 1 if you want the bar to be raised on top of other windows */
