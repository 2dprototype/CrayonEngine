import "canvas"
import "math"

// sizes := [16, 24, 32, 48, 64, 96, 128, 192, 256, 512]
sizes := [256, 512]

// ----- Palette ---------------------------------------------------------------
BG_COLOR     := "#1e2346"   // background circle
RED_COLOR    := "#ff3c46"   // red line
GREEN_COLOR  := "#5fd241"   // green line
BLUE_COLOR   := "#3c96ff"   // blue line
CENTER_COLOR := "#fff032"   // center hub dot

fn draw(s) {
	ctx := canvas.new_context(s, s)
	ctx.hex(BG_COLOR)
	// ctx.round_rect(0, 0, s, s, s/5)
	// ctx.circle(s/2, s/2, s/2)
	// ctx.fill()
	ctx.translate(-s/20, -s/20)
	
	var cx, cy = s/2, s/2
	
	// The green line goes from the center to (s/5, s/5).
	// Treat that as a vector and rotate it by ±120° to get the
	// red and blue lines — giving perfect three-fold symmetry.
	dx   := s/5 - cx
	dy   := s/5 - cy
	r    := math.sqrt(dx*dx + dy*dy)
	base := math.atan2(dy, dx)
	step := 2.0 * math.pi / 3.0
	
	ctx.line_width(s/8)
	
	// Green — original direction
	ctx.hex(GREEN_COLOR)
	ctx.move_to(cx, cy)
	ctx.line_to(cx + r*math.cos(base), cy + r*math.sin(base))
	ctx.stroke()
	
	// Red — green rotated by +120°
	ctx.hex(RED_COLOR)
	ctx.move_to(cx, cy)
	ctx.line_to(cx + r*math.cos(base + step), cy + r*math.sin(base + step))
	ctx.stroke()
	
	// Blue — green rotated by +240°
	ctx.hex(BLUE_COLOR)
	ctx.move_to(cx, cy)
	ctx.line_to(cx + r*math.cos(base + 2.0*step), cy + r*math.sin(base + 2.0*step))
	ctx.stroke()
	
	// Circle at the center, covering where the three lines meet
	ctx.hex(CENTER_COLOR)
	ctx.circle(cx, cy, s/20)
	ctx.fill()
	
	ctx.save_png(`icon_${s}.png`)
}

for size in sizes {
	draw(size)
}