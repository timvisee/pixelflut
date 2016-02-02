package de.paws.pixelwar;

import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.traffic.ChannelTrafficShapingHandler;

import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.apache.commons.lang3.StringUtils;

public class PixelClientHandler extends SimpleChannelInboundHandler<String> {

	public static class MessageShaper extends ChannelTrafficShapingHandler {
		public MessageShaper() {
			super(0, 1000, 1000);
		}

		@Override
		protected long calculateSize(final Object msg) {
			return 1;
		}
	}

	public interface CommandHandler {
		public void handle(PixelClientHandler client, String[] args);
	}

	private static Set<PixelClientHandler> clients = new HashSet<>();

	private final NetCanvas canvas;
	private ChannelHandlerContext channelContext;
	private final Set<String> subscriptions = new HashSet<>();
	private final Map<String, CommandHandler> handlers = new HashMap<>();

	private Label label;
	private boolean closed = false;

	private long pxcount = 0;

	public PixelClientHandler(final NetCanvas canvas) {
		this.canvas = canvas;
	}

	public void installHandler(final String command,
			final CommandHandler handler) {
		handlers.put(command.toUpperCase(), handler);
	}

	@Override
	public void channelActive(final ChannelHandlerContext ctx) throws Exception {

		super.channelActive(ctx);

		label = new Label();
		final SocketAddress raddr = ctx.channel().remoteAddress();
		if (raddr instanceof InetSocketAddress) {
			label.setText(((InetSocketAddress) raddr).getHostName());
		} else {
			label.setText(raddr.toString());
		}
		canvas.addDrawable(label);
		channelContext = ctx;
		synchronized (clients) {
			clients.add(this);
		}
	}

	@Override
	public void channelInactive(final ChannelHandlerContext ctx)
			throws Exception {
		super.channelActive(ctx);
		closed = true;
		synchronized (clients) {
			clients.remove(this);
		}
		label.setAlive(false);
	}

	public void writeIfPossible(final String str) {
		if (channelContext.channel().isWritable()) {
			write(str);
		} else {
			// error("Client too slow");
		}
	}

	private void writeChannel(final String channel, final String message) {
		if (subscriptions.contains(channel) || subscriptions.contains("*")) {
			writeIfPossible(message);
		}
	}

	public void write(final String str) {
		if (!closed) {
			channelContext.writeAndFlush(str + "\n");
		}
	}

	public void error(final String msg) {
		if (!closed) {
			channelContext.writeAndFlush("ERR " + msg + "\n").addListener(
					ChannelFutureListener.CLOSE);
		}
		closed = true;
	}

	@Override
	protected void channelRead0(final ChannelHandlerContext ctx,
			final String msg) throws Exception {
		final int split = msg.indexOf(" ");
		String command;
		String data;
		if (split == -1) {
			command = msg;
			data = "";
		} else {
			command = msg.substring(0, split);
			data = msg.substring(split + 1);
		}

		switch (command) {
		case ("PX"):
			handle_PX(ctx, data);
			break;
		case ("SIZE"):
			handle_SIZE(ctx, data);
			break;
		case ("PUB"):
			handle_PUB(ctx, data);
			break;
		case ("SUB"):
			handle_SUB(ctx, data);
			break;
		case ("HELP"):
			handle_HELP(ctx, data);
			break;
		default:
			error("Unknown command");
		}
	}

	private void handle_HELP(final ChannelHandlerContext ctx, final String data) {
		writeIfPossible("HELP Commands:\n" + "HELP  SIZE\n"
				+ "HELP  PX <x> <y>\n" + "HELP  PX <x> <y> <rrggbb[aa]>\n"
				+ "HELP  SUB\n" + "HELP  SUB [-]<channel>\n"
				+ "HELP  PUB <channel> <message>\n");
	}

	private void handle_SUB(final ChannelHandlerContext ctx, final String data) {
		if (data.startsWith("-")) {
			final String channel = data.substring(1);
			if (!subscriptions.remove(channel.toLowerCase())) {
				error("Not subscribed to this channel.");
			}
		} else if (data.length() > 0) {
			final String channel = data.toLowerCase();
			if (!channel.matches("^[a-zA-Z0-9_]+$")) {
				error("Invalid channel name");
			} else if (subscriptions.size() >= 16) {
				error("Too many subscriptions");
			} else {
				subscriptions.add(channel);
			}
		}

		write("SUB " + StringUtils.join(subscriptions, " "));
	}

	private void publish(final String channel, final String data) {
		final String message = "PUB " + channel.toLowerCase() + " "
				+ data.trim();

		for (final Object c : clients.toArray()) {
			((PixelClientHandler) c).writeChannel(channel, message);
		}
	}

	private void handle_PUB(final ChannelHandlerContext ctx, final String data) {
		final String[] parts = data.split(" ", 2);
		if (parts.length != 2) {
			error("Usage: PUB <channel> <message>");
			return;
		}

		publish(parts[0], parts[1]);
	}

	private void handle_SIZE(final ChannelHandlerContext ctx, final String data) {
		if (data.length() > 0) {
			error("SIZE");
		}
		write(String
				.format("SIZE %d %d", canvas.getWidth(), canvas.getHeight()));
	}

	private void handle_PX(final ChannelHandlerContext ctx, final String data) {
		final String[] args = data.split(" ");
		try {
			if (args.length == 2) {
				final int x = Integer.parseInt(args[0]);
				final int y = Integer.parseInt(args[1]);
				final int c = canvas.getPixel(x, y);
				writeIfPossible(String.format("PX %d %d %06x", x, y, c));
			} else if (args.length == 3) {
				pxcount++;
				final int x = Integer.parseInt(args[0]);
				final int y = Integer.parseInt(args[1]);
				int color = 0;
				if (args[2].length() == 6) {
					color = (int) Long.parseLong(args[2], 16) + 0xff000000;
				} else if (args[2].length() == 8) {
					color = (int) Long.parseLong(args[2], 16);
					color = (color >> 8) + ((color & 0xff) << 24);
				} else {
					error("Usage: PX x y [rrggbb[aa]]");
				}
				if (x >= 0 && y >= 0) {
					label.setPos(x, y);
					if ((color & 0xff000000) != 0) {
						canvas.setPixel(x, y, color);
						publish("px", data);
					}
				}
			} else {
				error("Usage: PX x y [rrggbb[aa]]");
			}
		} catch (final NumberFormatException e) {
			error("Usage: PX x y [rrggbb[aa]]");
		}
	}
}
