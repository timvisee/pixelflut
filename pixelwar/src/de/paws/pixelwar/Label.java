package de.paws.pixelwar;

import java.awt.Color;
import java.awt.FontMetrics;
import java.awt.Graphics2D;
import java.awt.geom.Rectangle2D;

public class Label extends Drawable {

	public static boolean show = true;

	float x = 0;
	float y = 0;
	int x2 = 0;
	int y2 = 0;
	String text = "";

	public String getText() {
		return text;
	}

	public void setText(final String text) {
		this.text = text;
	}

	public void setPos(final int x, final int y) {
		x2 = x;
		y2 = y;
	}

	@Override
	public void tick(final long dt) {
		x += ((x2 - x) * dt * 0.0001);
		y += ((y2 - y) * dt * 0.0001);
	}

	@Override
	public void draw(final Graphics2D g) {
		if (!show) {
			return;
		}

		final FontMetrics fm = g.getFontMetrics();
		final Rectangle2D rect = fm.getStringBounds(text, g);

		g.setColor(Color.BLACK);
		g.fillRect((int) x, (int) y - fm.getAscent(),
				(int) rect.getWidth() + 2, (int) rect.getHeight() + 2);
		g.setColor(Color.WHITE);
		g.drawString(text, (int) x, (int) y);
		g.drawLine((int) x, (int) y, x2, y2);
		g.drawOval(x2 - 5, y2 - 5, 10, 10);
	}

}
