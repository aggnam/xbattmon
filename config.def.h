char *colors[] = {
	[COLOR_BAT_CHARGED]     = "black",
	[COLOR_BAT_LEFT2CHARGE] = "white",
	[COLOR_BAT_DRAINED]     = "black",
	[COLOR_BAT_LEFT2DRAIN]  = "white"
};

unsigned int thickness = 2;	/* 2 pixels by default */
int placement = BOTTOM;		/* set to TOP if you want a top placement */
int maxcap = 100;		/* maximum battery capacity */
int raise = 0;			/* set to 1 if you want the bar to be raised on top of other windows */
int critical = 5;		/* start blinking below 5% */
int transparent = 0;		/* transparent mode */
