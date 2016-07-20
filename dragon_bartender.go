package main

import (
	"bufio"
	"flag"
	"fmt"
	"image"
	"image/draw"
	"math"
	"math/rand"
	"os"
	"path/filepath"
	"runtime"
	"time"

	"github.com/qeedquan/go-media/image/imageutil"
	"github.com/qeedquan/go-media/sdl"
	"github.com/qeedquan/go-media/sdl/sdlgfx"
	"github.com/qeedquan/go-media/sdl/sdlimage"
	"github.com/qeedquan/go-media/sdl/sdlimage/sdlcolor"
	"github.com/qeedquan/go-media/sdl/sdlttf"
)

const (
	WIDTH   = 800
	HEIGHT  = 600
	NUMBARS = 4
)

const (
	MAINMENU = iota
	INSTRUCTIONS
	INGAME
	PAUSE
	GAMEOVER
)

var (
	SAND   = sdl.Color{255, 193, 20, 255}
	PINK   = sdl.Color{255, 0, 255, 255}
	SKY    = sdl.Color{110, 255, 255, 255}
	DARKEN = sdl.Color{0, 0, 0, 92}
)

var (
	conf struct {
		assets     string
		pref       string
		fullscreen bool
		invincible bool
	}

	window   *sdl.Window
	renderer *sdl.Renderer
	canvas   *image.RGBA
	surface  *sdl.Surface
	texture  *sdl.Texture
	fps      sdlgfx.FPSManager

	ctls []*sdl.GameController

	images       = make(map[string]*Image)
	background   *Image
	title        *Image
	instructions *Image
	dragonIcon   *Image
	bartop       *Image
	font         *sdlttf.Font
	bfont        *sdlttf.Font

	state       int
	cursor      int
	axisDelay   uint32
	stat        Stat
	dragon      *Dragon
	projectiles []*Projectile
	guys        []*Guy
	cactus      [3]*Cactus
	spawner     int

	saveError error
	saved     bool
	loadError error
)

func main() {
	runtime.LockOSThread()
	parseFlags()
	initSDL()
	loadAssets()

	reset()
	for {
		event()
		update()
		blit()
	}
}

func parseFlags() {
	conf.assets = filepath.Join(sdl.GetBasePath(), "assets")
	conf.pref = sdl.GetPrefPath("", "dragon_bartender")
	flag.StringVar(&conf.assets, "assets", conf.assets, "assets directory")
	flag.StringVar(&conf.pref, "pref", conf.pref, "preference directory")
	flag.BoolVar(&conf.fullscreen, "fullscreen", false, "fullscreen mode")
	flag.BoolVar(&conf.invincible, "invincible", false, "be invincible")
	flag.Parse()
}

func initSDL() {
	err := sdl.Init(sdl.INIT_EVERYTHING &^ sdl.INIT_AUDIO)
	ck(err)

	err = sdlttf.Init()
	ck(err)

	sdl.SetHint(sdl.HINT_RENDER_SCALE_QUALITY, "best")

	w, h := WIDTH, HEIGHT
	wflag := sdl.WINDOW_RESIZABLE
	if conf.fullscreen {
		wflag |= sdl.WINDOW_FULLSCREEN_DESKTOP
	}
	window, renderer, err = sdl.CreateWindowAndRenderer(w, h, wflag)
	ck(err)

	window.SetTitle("Old West Railroad Dragon Bartender!")
	renderer.SetLogicalSize(w, h)

	texture, err = renderer.CreateTexture(sdl.PIXELFORMAT_ABGR8888, sdl.TEXTUREACCESS_STREAMING, w, h)
	ck(err)

	canvas = image.NewRGBA(image.Rect(0, 0, w, h))

	surface, err = sdl.CreateRGBSurface(sdl.SWSURFACE, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
	ck(err)

	mapControllers()

	sdl.ShowCursor(0)

	fps.Init()
	fps.SetRate(60)

	axisDelay = sdl.GetTicks()
}

func mapControllers() {
	for _, c := range ctls {
		if c != nil {
			c.Close()
		}
	}

	ctls = make([]*sdl.GameController, sdl.NumJoysticks())
	for i, _ := range ctls {
		if sdl.IsGameController(i) {
			var err error
			ctls[i], err = sdl.GameControllerOpen(i)
			ek(err)
		}
	}
}

func loadAssets() {
	setIcon("dragonicon.png")
	background = loadImage("background.png")
	title = loadImage("title.png")
	instructions = loadImage("instructions.png")
	dragonIcon = loadImage("dragonicon.png")
	bartop = loadImage("bartop.png")
	font = loadFont("FreeMono.ttf", 16)
	bfont = loadFont("FreeMono.ttf", 42)
}

func reset() {
	rand.Seed(time.Now().UnixNano())
	stat.Reset()
	for i := range cactus {
		cactus[i] = newCactus(i, 0)
	}
	dragon = newDragon()
	loadError = nil
	saveError = nil
	saved = false
	state = MAINMENU
}

func evMainMenu(ev sdl.Event) {
	up := func() {
		if cursor--; cursor < 0 {
			cursor = 2
		}
	}
	down := func() {
		if cursor++; cursor > 2 {
			cursor = 0
		}
	}
	enter := func() {
		switch cursor {
		case 1:
			reset()
			loadError = loadGame()
			if !ek(loadError) {
				state = PAUSE
			}
		case 2:
			state = INSTRUCTIONS
		default:
			reset()
			state = INGAME
		}
	}

	switch ev := ev.(type) {
	case sdl.KeyDownEvent:
		switch ev.Sym {
		case sdl.K_RETURN, sdl.K_SPACE:
			enter()
		case sdl.K_UP:
			up()
		case sdl.K_DOWN:
			down()
		}

	case sdl.ControllerButtonDownEvent:
		button := sdl.GameControllerButton(ev.Button)
		switch button {
		case sdl.CONTROLLER_BUTTON_DPAD_UP:
			up()
		case sdl.CONTROLLER_BUTTON_DPAD_DOWN:
			down()
		case sdl.CONTROLLER_BUTTON_X, sdl.CONTROLLER_BUTTON_Y,
			sdl.CONTROLLER_BUTTON_A, sdl.CONTROLLER_BUTTON_B:
			enter()
		}

	case sdl.ControllerAxisEvent:
		if ev.Timestamp-axisDelay < 100 {
			return
		}
		axisDelay = ev.Timestamp
		const threshold = 1000
		switch {
		case sdl.GameControllerAxis(ev.Axis) == sdl.CONTROLLER_AXIS_LEFTY && ev.Value < -threshold:
			up()
		case sdl.GameControllerAxis(ev.Axis) == sdl.CONTROLLER_AXIS_LEFTY && ev.Value > threshold:
			down()
		}
	}
}

func evInstructions(ev sdl.Event) {
	switch ev := ev.(type) {
	case sdl.KeyDownEvent:
		switch ev.Sym {
		case sdl.K_RETURN, sdl.K_SPACE, sdl.K_p:
			state = MAINMENU
		}

	case sdl.ControllerButtonDownEvent:
		button := sdl.GameControllerButton(ev.Button)
		switch button {
		case sdl.CONTROLLER_BUTTON_X, sdl.CONTROLLER_BUTTON_Y,
			sdl.CONTROLLER_BUTTON_A, sdl.CONTROLLER_BUTTON_B:
			state = MAINMENU
		}
	}
}

func evInGame(ev sdl.Event) {
	up := func() {
		if dragon.Bar > 0 {
			dragon.Bar--
		}
	}
	down := func() {
		if dragon.Bar < NUMBARS-1 {
			dragon.Bar++
		}
	}
	fire := func(typ int) {
		projectiles = append(projectiles, newProjectile(typ, dragon.X, dragon.Bar))
	}

	switch ev := ev.(type) {
	case sdl.KeyDownEvent:
		switch ev.Sym {
		case sdl.K_SPACE, sdl.K_p:
			state = PAUSE
		case sdl.K_DOWN:
			down()
		case sdl.K_UP:
			up()
		case sdl.K_z:
			fire(SHOTGLASS)
		case sdl.K_x:
			fire(FIREBALL)
		case sdl.K_i:
			toggleInvincible()
		case sdl.K_r:
			state = MAINMENU
		}

	case sdl.ControllerButtonDownEvent:
		button := sdl.GameControllerButton(ev.Button)
		switch button {
		case sdl.CONTROLLER_BUTTON_START:
			state = PAUSE
		case sdl.CONTROLLER_BUTTON_DPAD_UP:
			up()
		case sdl.CONTROLLER_BUTTON_DPAD_DOWN:
			down()
		case sdl.CONTROLLER_BUTTON_A:
			fire(SHOTGLASS)
		case sdl.CONTROLLER_BUTTON_B:
			fire(FIREBALL)
		case sdl.CONTROLLER_BUTTON_X:
			toggleInvincible()
		case sdl.CONTROLLER_BUTTON_Y:
			state = MAINMENU
		}

	case sdl.ControllerAxisEvent:
		if ev.Timestamp-axisDelay < 50 {
			return
		}
		axisDelay = ev.Timestamp
		const threshold = 1000
		switch {
		case sdl.GameControllerAxis(ev.Axis) == sdl.CONTROLLER_AXIS_LEFTY && ev.Value < -threshold:
			up()
		case sdl.GameControllerAxis(ev.Axis) == sdl.CONTROLLER_AXIS_LEFTY && ev.Value > threshold:
			down()
		}
	}
}

func toggleInvincible() {
	fmt.Fprintln(os.Stderr, "invinciblity:", conf.invincible)
	conf.invincible = !conf.invincible
}

func evPause(ev sdl.Event) {
	save := func() {
		if saved {
			return
		}
		saveError = saveGame()
		ek(saveError)
		saved = true
	}
	switch ev := ev.(type) {
	case sdl.KeyDownEvent:
		switch ev.Sym {
		case sdl.K_SPACE, sdl.K_p:
			state = INGAME
		case sdl.K_s:
			save()
		case sdl.K_r:
			state = MAINMENU
		}

	case sdl.ControllerButtonDownEvent:
		button := sdl.GameControllerButton(ev.Button)
		switch button {
		case sdl.CONTROLLER_BUTTON_START:
			state = INGAME
		case sdl.CONTROLLER_BUTTON_X:
			save()
		case sdl.CONTROLLER_BUTTON_Y:
			state = MAINMENU
		}
	}
}

func evGameOver() {
	state = MAINMENU
	reset()
}

func event() {
	for {
		ev := sdl.PollEvent()
		if ev == nil {
			break
		}
		switch ev := ev.(type) {
		case sdl.QuitEvent:
			os.Exit(0)
		case sdl.KeyDownEvent:
			switch ev.Sym {
			case sdl.K_ESCAPE:
				os.Exit(0)
			}
		case sdl.ControllerButtonDownEvent:
			button := sdl.GameControllerButton(ev.Button)
			switch button {
			case sdl.CONTROLLER_BUTTON_BACK:
				os.Exit(0)
			}
		case sdl.ControllerDeviceAddedEvent:
			mapControllers()
			continue
		}

		switch state {
		case MAINMENU:
			evMainMenu(ev)
		case INSTRUCTIONS:
			evInstructions(ev)
		case INGAME:
			evInGame(ev)
		case PAUSE:
			evPause(ev)
		case GAMEOVER:
			evGameOver()
		}
	}
}

func update() {
	if state != INGAME {
		return
	}

	for _, c := range cactus {
		c.Update()
	}

	if spawner <= 0 {
		guys = append(guys, newGuy())
		stat.Seen++
		spawner = 100
		if spawner < 5 {
			spawner = 5
		}
	} else {
		spawner--
	}

	for i := 0; i < len(guys); {
		g := guys[i]
		g.Walk()
		if g.X > 545 {
			n := len(guys) - 1
			guys[i], guys = guys[n], guys[:n]

			if !conf.invincible {
				if stat.Lives--; stat.Lives <= 0 {
					state = GAMEOVER
				}
			}
		} else {
			i++
		}
	}

	for i := 0; i < len(projectiles); {
		p := projectiles[i]
		p.Move()

		hit := false
		for j := 0; j < len(guys); j++ {
			g := guys[j]
			if p.Bar == g.Bar && p.X > g.X && p.X < g.X+60 {
				goodShot := false
				switch {
				case g.Type == CUSTOMER && p.Type == SHOTGLASS,
					g.Type == BANDIT && p.Type == FIREBALL:
					goodShot = true
				}
				updateScore(goodShot)

				n := len(guys) - 1
				guys[j], guys = guys[n], guys[:n]

				n = len(projectiles) - 1
				projectiles[i], projectiles = projectiles[n], projectiles[:n]

				break
			}
		}

		if hit {
			continue
		}

		if p.X < -10 {
			n := len(projectiles) - 1
			projectiles[i], projectiles = projectiles[n], projectiles[:n]
		} else {
			i++
		}
	}
}

func updateScore(goodShot bool) {
	if goodShot {
		stat.Score += stat.Multiplier
		if stat.Hits++; stat.Hits >= stat.Prime {
			stat.Multiplier++
			if stat.Count += 2; stat.Count > len(primes) {
				stat.Count = primes[len(primes)-1]
			}
			stat.Hits = 0
			stat.Prime = primes[stat.Count]
		}
	} else {
		stat.Multiplier = 1
		stat.Hits = 0
		stat.Count = 3
		stat.Prime = primes[stat.Count]
	}
}

func blitText(font *sdlttf.Font, fg sdl.Color, x, y int, format string, args ...interface{}) {
	text := fmt.Sprintf(format, args...)
	r, err := font.RenderUTF8BlendedEx(surface, text, fg)
	ck(err)
	draw.Draw(canvas, image.Rect(x, y, x+int(r.W), y+int(r.H)), surface, image.ZP, draw.Over)
}

func blitTextCenter(font *sdlttf.Font, fg sdl.Color, x, y int, format string, args ...interface{}) {
	text := fmt.Sprintf(format, args...)
	r, err := font.RenderUTF8BlendedEx(surface, text, fg)
	ck(err)
	x -= int(r.W) / 2
	draw.Draw(canvas, image.Rect(x, y, x+int(r.W), y+int(r.H)), surface, image.ZP, draw.Over)
}

func blitMainMenu() {
	title.Blit(222, 100)
	blitText(bfont, sdlcolor.Black, WIDTH/2-100, HEIGHT-220, "Start")
	if loadError == nil {
		blitText(bfont, sdlcolor.Black, WIDTH/2-100, HEIGHT-170, "Continue")
	} else {
		blitText(bfont, sdlcolor.Black, WIDTH/2-100, HEIGHT-170, "Error")
	}
	blitText(bfont, sdlcolor.Black, WIDTH/2-100, HEIGHT-120, "Instructions")
	dragonIcon.Blit(WIDTH/2-200, HEIGHT-220+50*float64(cursor))
}

func blitInGame() {
	draw.Draw(canvas, image.Rect(0, 0, WIDTH, 7), image.NewUniform(SKY), image.ZP, draw.Over)
	for _, c := range cactus {
		c.Blit(c.X, c.Y)
	}
	background.Blit(0, 0)

	dragonIcon.Blit(WIDTH-230, 0)
	blitText(bfont, sdlcolor.Black, WIDTH-130, 0, "x %d", stat.Lives)

	font.SetStyle(sdlttf.STYLE_BOLD)
	blitText(font, sdlcolor.Black, 0, 0, "Score: %d     Multiplier: x%d  (%d until next)",
		stat.Score, stat.Multiplier, stat.Prime-stat.Hits)

	const barStartY = 110
	const barSpaceY = 120
	for i := 0; i < NUMBARS; i++ {
		for _, g := range guys {
			if g.Bar == i {
				g.Blit()
			}
		}
		bartop.Blit(0, float64(barStartY+barSpaceY*i))
		for _, p := range projectiles {
			p.Blit()
		}
	}

	const dragonBarYoff = -50
	dragon.Blit(dragon.X, float64(dragonBarYoff+barStartY+barSpaceY*dragon.Bar))
}

func blitPause() {
	blitInGame()
	draw.Draw(canvas, canvas.Bounds(), image.NewUniform(DARKEN), image.ZP, draw.Over)

	centerY := HEIGHT/2 - 1 - bfont.Height()
	bottomY := HEIGHT - 50 - bfont.Height()
	blitTextCenter(bfont, sdlcolor.White, WIDTH/2, centerY, "PAUSED")
	if saveError != nil {
		blitTextCenter(bfont, sdlcolor.White, WIDTH/2, bottomY, "Error saving game")
	} else if saved {
		blitTextCenter(bfont, sdlcolor.White, WIDTH/2, bottomY, "Game saved")
	} else {
		blitTextCenter(bfont, sdlcolor.White, WIDTH/2, bottomY, "Press S to save the game")
	}
}

func blitGameOver() {
	blitInGame()
	draw.Draw(canvas, canvas.Bounds(), image.NewUniform(DARKEN), image.ZP, draw.Over)

	centerY := HEIGHT/2 - 1 - bfont.Height()
	blitTextCenter(bfont, sdlcolor.White, WIDTH/2, centerY, "GAME OVER")
	blitTextCenter(bfont, sdlcolor.White, WIDTH/2, centerY+50, "Your final score was %d", stat.Score)
}

func blit() {
	draw.Draw(canvas, canvas.Bounds(), image.NewUniform(SAND), image.ZP, draw.Src)
	switch state {
	case MAINMENU:
		blitMainMenu()
	case INSTRUCTIONS:
		instructions.Blit(0, 0)
	case INGAME:
		blitInGame()
	case PAUSE:
		blitPause()
	case GAMEOVER:
		blitGameOver()
	}

	renderer.SetDrawColor(sdlcolor.Black)
	renderer.Clear()
	texture.Update(nil, canvas.Pix, canvas.Stride)
	renderer.Copy(texture, nil, nil)
	renderer.Present()
}

func saveGame() error {
	name := filepath.Join(conf.pref, "savedgame")
	f, err := os.Create(name)
	if err != nil {
		return err
	}

	w := bufio.NewWriter(f)
	fmt.Fprintln(w, stat.Score)
	fmt.Fprintln(w, stat.Multiplier)
	fmt.Fprintln(w, stat.Seen)
	fmt.Fprintln(w, stat.Hits)
	fmt.Fprintln(w, stat.Prime)
	fmt.Fprintln(w, stat.Count)
	fmt.Fprintln(w, stat.Lives)
	fmt.Fprintln(w, dragon.Bar)
	fmt.Fprintln(w, cactus[CACTUS].X)
	fmt.Fprintln(w, cactus[SMALL_CACTUS].X)
	fmt.Fprintln(w, cactus[TINY_CACTUS].X)

	fmt.Fprintln(w, len(guys))
	for _, g := range guys {
		fmt.Fprintln(w, g.Type, g.X, g.Bar, g.Yoff)
	}

	fmt.Fprintln(w, len(projectiles))
	for _, p := range projectiles {
		fmt.Fprintln(w, p.Type, p.X, p.Bar)
	}

	err = w.Flush()
	xerr := f.Close()
	if err == nil {
		err = xerr
	}
	return err
}

func loadGame() error {
	name := filepath.Join(conf.pref, "savedgame")
	f, err := os.Open(name)
	if err != nil {
		return err
	}
	defer f.Close()

	r := bufio.NewReader(f)
	fmt.Fscan(r, &stat.Score)
	fmt.Fscan(r, &stat.Multiplier)
	fmt.Fscan(r, &stat.Seen)
	fmt.Fscan(r, &stat.Hits)
	fmt.Fscan(r, &stat.Prime)
	fmt.Fscan(r, &stat.Count)
	fmt.Fscan(r, &stat.Lives)
	fmt.Fscan(r, &dragon.Bar)
	fmt.Fscan(r, &cactus[CACTUS].X)
	fmt.Fscan(r, &cactus[SMALL_CACTUS].X)
	fmt.Fscan(r, &cactus[TINY_CACTUS].X)

	var numGuys int
	fmt.Fscan(r, &numGuys)
	guys = make([]*Guy, numGuys)
	for i := range guys {
		var typ, bar int
		var x, yoff float64

		fmt.Fscan(r, &typ, &x, &bar, &yoff)
		guys[i] = newGuyEx(typ, x, bar, yoff)
	}

	var numProjectiles int
	fmt.Fscan(r, &numProjectiles)
	projectiles = make([]*Projectile, numProjectiles)
	for i := range projectiles {
		var typ, bar int
		var x float64

		fmt.Fscan(r, &typ, &x, &bar)
		projectiles[i] = newProjectile(typ, x, bar)
	}

	return nil
}

func ck(err error) {
	if err != nil {
		sdl.LogCritical(sdl.LOG_CATEGORY_APPLICATION, "%v", err)
		sdl.ShowSimpleMessageBox(sdl.MESSAGEBOX_ERROR, "Error", err.Error(), window)
		os.Exit(1)
	}
}

func ek(err error) bool {
	if err != nil {
		sdl.LogError(sdl.LOG_CATEGORY_APPLICATION, "%v", err)
		return true
	}
	return false
}

type Stat struct {
	Score      int
	Multiplier int
	Seen       int
	Hits       int
	Prime      int
	Count      int
	Lives      int
}

func (s *Stat) Reset() {
	*s = Stat{
		Multiplier: 1,
		Prime:      primes[3],
		Count:      3,
		Lives:      3,
	}
}

type Image struct {
	*image.RGBA
	W, H int
}

func loadImage(name string) *Image {
	name = filepath.Join(conf.assets, name)
	if img := images[name]; img != nil {
		return img
	}

	rgba, err := imageutil.LoadFile(name)
	ck(err)
	rgba = imageutil.ColorKey(rgba, PINK)

	bounds := rgba.Bounds()

	img := &Image{rgba, bounds.Dx(), bounds.Dy()}
	images[name] = img

	return img
}

func loadFont(name string, ptSize int) *sdlttf.Font {
	name = filepath.Join(conf.assets, name)
	font, err := sdlttf.OpenFont(name, ptSize)
	ck(err)
	return font
}

func setIcon(name string) {
	name = filepath.Join(conf.assets, name)
	rgba, err := imageutil.LoadFile(name)
	if ek(err) {
		return
	}
	rgba = imageutil.ColorKey(rgba, PINK)
	surface, err := sdlimage.LoadSurfaceImage(rgba)
	if ek(err) {
		return
	}
	defer surface.Free()
	window.SetIcon(surface)
}

func (m *Image) Blit(x, y float64) {
	draw.Draw(canvas, image.Rect(int(x), int(y), int(x)+m.W, int(y)+m.H), m.RGBA, image.ZP, draw.Over)
}

type Entity struct {
	*Image
	Type int
	X    float64
	Bar  int
}

type Dragon struct {
	Entity
}

func newDragon() *Dragon {
	return &Dragon{
		Entity: Entity{
			Image: loadImage("dragon.png"),
			X:     535,
		},
	}
}

const (
	SHOTGLASS = iota
	FIREBALL
)

type Projectile struct {
	Entity
}

func newProjectile(typ int, x float64, bar int) *Projectile {
	var img *Image
	switch typ {
	case SHOTGLASS:
		img = loadImage("shotglass.png")
	case FIREBALL:
		img = loadImage("fireball.png")
	}
	return &Projectile{
		Entity: Entity{
			Image: img,
			Type:  typ,
			X:     x,
			Bar:   bar,
		},
	}
}

func (p *Projectile) Move() {
	switch p.Type {
	case FIREBALL:
		p.X -= 10
	default:
		p.X -= 6
	}
}

func (p *Projectile) Blit() {
	var y float64
	switch p.Type {
	case SHOTGLASS:
		y = 110 + float64(p.Bar)*120
	case FIREBALL:
		y = 80 + float64(p.Bar)*120
	}
	p.Entity.Blit(p.X, y)
}

const (
	CUSTOMER = iota
	BANDIT
)

type Guy struct {
	Entity
	Yoff float64
}

func newGuy() *Guy {
	typ := CUSTOMER
	if rand.Int()%4 == 0 {
		typ = BANDIT
	}
	return newGuyEx(typ, -50, rand.Intn(4), math.Pi*rand.Float64())
}

func newGuyEx(typ int, x float64, bar int, yoff float64) *Guy {
	img := loadImage("customer.png")
	if typ == BANDIT {
		img = loadImage("bandit.png")
	}
	return &Guy{
		Entity: Entity{
			Image: img,
			Type:  typ,
			X:     x,
			Bar:   bar,
		},
		Yoff: yoff,
	}
}

func (g *Guy) Walk() {
	g.X += 2.5
	g.Yoff += math.Pi / 20
	if g.Yoff > math.Pi {
		g.Yoff -= math.Pi
	}
}

func (g *Guy) Blit() {
	y := 50 + 120*float64(g.Bar) + 5*math.Sin(g.Yoff)
	switch g.Type {
	case BANDIT:
		g.Entity.Blit(g.X, y)
	case CUSTOMER:
		g.Entity.Blit(g.X, y)
	}
}

const (
	TINY_CACTUS = iota
	SMALL_CACTUS
	CACTUS
)

type Cactus struct {
	Entity
	Y     float64
	Speed float64
	Size  float64
}

func newCactus(typ int, x float64) *Cactus {
	var img *Image
	var y, speed, size float64
	switch typ {
	case TINY_CACTUS:
		img = loadImage("cactus3.png")
		y = -5
		speed = -1.5
		size = 35
	case SMALL_CACTUS:
		img = loadImage("cactus2.png")
		y = 0
		speed = -3
		size = 70
	case CACTUS:
		img = loadImage("cactus1.png")
		y = 10
		speed = -5
		size = 140
	}
	return &Cactus{
		Entity: Entity{
			Image: img,
			X:     x,
		},
		Y:     y,
		Speed: speed,
		Size:  size,
	}
}

func (c *Cactus) Update() {
	c.X += c.Speed
	if c.X+c.Size < 0 {
		c.X = WIDTH
	}
}

var primes = []int{
	2, 3, 5, 7, 11, 13, 17, 19, 23, 29,
	31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
	73, 79, 83, 89, 97, 101, 103, 107, 109, 113,
	127, 131, 137, 139, 149, 151, 157, 163, 167, 173,
	179, 181, 191, 193, 197, 199, 211, 223, 227, 229,
	233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
	283, 293, 307, 311, 313, 317, 331, 337, 347, 349,
	353, 359, 367, 373, 379, 383, 389, 397, 401, 409,
	419, 421, 431, 433, 439, 443, 449, 457, 461, 463,
	467, 479, 487, 491, 499, 503, 509, 521, 523, 541,
	547, 557, 563, 569, 571, 577, 587, 593, 599, 601,
	607, 613, 617, 619, 631, 641, 643, 647, 653, 659,
	661, 673, 677, 683, 691, 701, 709, 719, 727, 733,
	739, 743, 751, 757, 761, 769, 773, 787, 797, 809,
	811, 821, 823, 827, 829, 839, 853, 857, 859, 863,
	877, 881, 883, 887, 907, 911, 919, 929, 937, 941,
	947, 953, 967, 971, 977, 983, 991, 997, 1009,
}
