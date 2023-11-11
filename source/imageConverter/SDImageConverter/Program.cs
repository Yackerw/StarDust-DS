using System;
using System.IO;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;

namespace SDImageConverter
{
	class Program
	{

		private struct PaletteImage
		{
			public List<int> pixelIndexes;
			public List<Color> palette;
			public List<int> alpha;
			public int width;
			public int height;
		}
		private static PaletteImage PaletteifyImage(Bitmap img, int size, bool alphaEnabled = false)
		{
			List<Color> usedColors = new List<Color>();
			List<int> pixelIndexes = new List<int>();
			List<int> pixelAlpha = new List<int>();
			int imgSize = img.Width * img.Height;
			for (int i = 0; i < img.Height; ++i)
			{
				for (int i2 = 0; i2 < img.Width; ++i2)
				{
					Color col = img.GetPixel(i2, i);
					// compare it against all previously established colors
					bool sameColor = false;
					byte alpha = col.A >= 127 ? (byte)255 : (byte)0;
					if (alphaEnabled)
					{
						pixelAlpha.Add(col.A);	
					}
					col = Color.FromArgb(alpha, (col.R >> 3) << 3, (col.G >> 3) << 3, (col.B >> 3) << 3);
					for (int i3 = 0; i3 < usedColors.Count; ++i3)
					{
						// condense all 0 alpha pixels to one
						if (usedColors[i3] == col || usedColors[i3].A == 0 && col.A == 0)
						{
							sameColor = true;
							pixelIndexes.Add(i3);
							i3 = usedColors.Count;
						}
					}
					if (!sameColor)
					{
						usedColors.Add(col);
						pixelIndexes.Add(usedColors.Count - 1);
					}
				}
			}

			// okay great, now get the most similar ones until we're within space
			while (usedColors.Count > size)
			{
				int lowestDelta = 1024;
				int closest1 = 0;
				int closest2 = 0;
				for (int i = 0; i < usedColors.Count; ++i)
				{
					for (int i2 = i + 1; i2 < usedColors.Count; ++i2)
					{
						// let's actually delta in converted to A3RGB5
						int deltaColor = Math.Abs((usedColors[i].R >> 3) - (usedColors[i2].R >> 3));
						deltaColor += Math.Abs((usedColors[i].G >> 3) - (usedColors[i2].G >> 3));
						deltaColor += Math.Abs((usedColors[i].B >> 3) - (usedColors[i2].B >> 3));
						// make alpha really heavy for determining
						deltaColor += Math.Abs((usedColors[i].A) - (usedColors[i2].A)) * 768;
						if (deltaColor <= lowestDelta)
						{
							lowestDelta = deltaColor;
							closest1 = i;
							closest2 = i2;
						}
					}
				}
				// okay, now average out and then pop the closest colors
				usedColors[closest1] = Color.FromArgb(
					((int)usedColors[closest1].A + (int)usedColors[closest2].A) / 2,
					((int)usedColors[closest1].R + (int)usedColors[closest2].R) / 2,
					((int)usedColors[closest1].G + (int)usedColors[closest2].G) / 2,
					((int)usedColors[closest1].B + (int)usedColors[closest2].B) / 2);
				// adjust pixel indexes
				for (int i = 0; i < pixelIndexes.Count; ++i)
				{
					pixelIndexes[i] = ((pixelIndexes[i] == closest2) ? closest1 : pixelIndexes[i]);
					if (pixelIndexes[i] > closest2)
					{
						--pixelIndexes[i];
					}
				}
				usedColors.RemoveAt(closest2);
			}

			// ensure it meets a minimum size
			/*while (usedColors.Count < size)
			{
				usedColors.Add(new Color());
			}*/

			PaletteImage PI = new PaletteImage();
			PI.pixelIndexes = pixelIndexes;
			PI.palette = usedColors;
			PI.width = img.Width;
			PI.height = img.Height;
			PI.alpha = pixelAlpha;
			return PI;
		}

		static void CreatePC32(string fileName, int UType, int VType, bool retain)
		{
			Bitmap fileToConvert;
			try
			{
				fileToConvert = (Bitmap)Bitmap.FromFile(fileName);
			}
			catch
			{
				Console.WriteLine("Couldn't open file " + fileName);
				return;
			}
			PaletteImage pi = PaletteifyImage(fileToConvert, 32, true);
			int width = 0;
			int height = 0;
			switch (pi.width)
			{
				case 8:
					width = 0;
					break;
				case 16:
					width = 1;
					break;
				case 32:
					width = 2;
					break;
				case 64:
					width = 3;
					break;
				case 128:
					width = 4;
					break;
				case 256:
					width = 5;
					break;
				case 512:
					width = 6;
					break;
				case 1024:
					width = 7;
					break;
				default:
					Console.WriteLine("Image dimensions must be power of 2!");
					return;
					break;
			}
			switch (pi.height)
			{
				case 8:
					height = 0;
					break;
				case 16:
					height = 1;
					break;
				case 32:
					height = 2;
					break;
				case 64:
					height = 3;
					break;
				case 128:
					height = 4;
					break;
				case 256:
					height = 5;
					break;
				case 512:
					height = 6;
					break;
				case 1024:
					height = 7;
					break;
				default:
					Console.WriteLine("Image dimensions must be power of 2!");
					return;
					break;
			}
			byte[] processedData = new byte[pi.pixelIndexes.Count];
			for (int i = 0; i < pi.pixelIndexes.Count; ++i)
			{
				processedData[i] = (byte)((byte)(pi.pixelIndexes[i]) | (byte)((pi.alpha[i] >> 5) << 5));
			}
			// palette data now
			ushort[] paletteData = ConvertPalette(pi);
			if (paletteData.Length < 32)
			{
				Array.Resize(ref paletteData, 32);
			}
			// write data now
			FileStream fs = File.Open(fileName.Substring(0, fileName.Length - 3) + "sdi", FileMode.Create);
			fs.WriteByte(1);
			fs.WriteByte((byte)width);
			fs.WriteByte((byte)height);
			// whether or not it remains in ram
			fs.WriteByte(retain ? (byte)1 : (byte)0);
			//  reference count
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// UV types
			fs.WriteByte((byte)UType);
			fs.WriteByte((byte)VType);
			// padding
			fs.WriteByte((byte)0);
			fs.WriteByte((byte)0);

			// palette pointer
			fs.Write(BitConverter.GetBytes((int)0x30));
			// image data pointer
			fs.Write(BitConverter.GetBytes(0x30 + (paletteData.Length * 2)));
			// char *texturename
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// previous (0x10)
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// next (0x14)
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// int texture id, int palette id
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// padding
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// palette now
			for (int i = 0; i < paletteData.Length; ++i)
			{
				fs.Write(BitConverter.GetBytes(paletteData[i]), 0, 2);
			}
			// image data now
			fs.Write(processedData, 0, processedData.Length);
			fs.Close();
			return;
		}

		static void CreatePC16(string fileName, int UType, int VType, bool retain)
		{
			Bitmap fileToConvert;
			try
			{
				fileToConvert = (Bitmap)Bitmap.FromFile(fileName);
			}
			catch
			{
				Console.WriteLine("Couldn't open file " + fileName);
				return;
			}
			int width = 0;
			int height = 0;
			switch (fileToConvert.Width)
			{
				case 8:
					width = 0;
					break;
				case 16:
					width = 1;
					break;
				case 32:
					width = 2;
					break;
				case 64:
					width = 3;
					break;
				case 128:
					width = 4;
					break;
				case 256:
					width = 5;
					break;
				case 512:
					width = 6;
					break;
				case 1024:
					width = 7;
					break;
				default:
					Console.WriteLine("Image dimensions must be power of 2!");
					return;
					break;
			}
			switch (fileToConvert.Height)
			{
				case 8:
					height = 0;
					break;
				case 16:
					height = 1;
					break;
				case 32:
					height = 2;
					break;
				case 64:
					height = 3;
					break;
				case 128:
					height = 4;
					break;
				case 256:
					height = 5;
					break;
				case 512:
					height = 6;
					break;
				case 1024:
					height = 7;
					break;
				default:
					Console.WriteLine("Image dimensions must be power of 2!");
					return;
					break;
			}
			ushort[] colorData = new ushort[fileToConvert.Width * fileToConvert.Height];
			for (int i = 0; i < fileToConvert.Height; ++i)
			{
				for (int i2 = 0; i2 < fileToConvert.Width; ++i2)
				{
					Color currPixel = fileToConvert.GetPixel(i2, i);
					ushort currentColor = 0;
					if (currPixel.A >= 127)
					{
						currentColor = 32768;
					}
					currentColor |= (ushort)((currPixel.B >> 3) << 10);
					currentColor |= (ushort)((currPixel.G >> 3) << 5);
					currentColor |= (ushort)(currPixel.R >> 3);
					colorData[(i*fileToConvert.Width) + i2] = currentColor;
				}
			}
			// now write the file
			FileStream fs = File.Open(fileName.Substring(0, fileName.Length - 3) + "sdi", FileMode.Create);
			fs.WriteByte(7);
			fs.WriteByte((byte)width);
			fs.WriteByte((byte)height);
			// whether or not it remains in ram
			// whether or not it remains in ram
			fs.WriteByte(retain ? (byte)1 : (byte)0);
			//  reference count
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// UV types
			fs.WriteByte((byte)UType);
			fs.WriteByte((byte)VType);
			// padding
			fs.WriteByte((byte)0);
			fs.WriteByte((byte)0);

			// palette pointer
			fs.Write(BitConverter.GetBytes((int)0));
			// image data pointer
			fs.Write(BitConverter.GetBytes((int)0x30));
			// char *texturename
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// previous (0x10)
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// next (0x14)
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// int texture id, int palette id
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// padding
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// image data now
			for (int i = 0; i < colorData.Length; ++i)
			{
				fs.Write(BitConverter.GetBytes(colorData[i]));
			}
			fs.Close();
		}

		static void CreatePC8(string fileName, int UType, int VType, bool retain)
		{
			Bitmap fileToConvert;
			try
			{
				fileToConvert = (Bitmap)Bitmap.FromFile(fileName);
			}
			catch
			{
				Console.WriteLine("Couldn't open file " + fileName);
				return;
			}
			PaletteImage pi = PaletteifyImage(fileToConvert, 256);
			int width = 0;
			int height = 0;
			switch (pi.width)
			{
				case 8:
					width = 0;
					break;
				case 16:
					width = 1;
					break;
				case 32:
					width = 2;
					break;
				case 64:
					width = 3;
					break;
				case 128:
					width = 4;
					break;
				case 256:
					width = 5;
					break;
				case 512:
					width = 6;
					break;
				case 1024:
					width = 7;
					break;
				default:
					Console.WriteLine("Image dimensions must be power of 2!");
					return;
					break;
			}
			switch (pi.height)
			{
				case 8:
					height = 0;
					break;
				case 16:
					height = 1;
					break;
				case 32:
					height = 2;
					break;
				case 64:
					height = 3;
					break;
				case 128:
					height = 4;
					break;
				case 256:
					height = 5;
					break;
				case 512:
					height = 6;
					break;
				case 1024:
					height = 7;
					break;
				default:
					Console.WriteLine("Image dimensions must be power of 2!");
					return;
					break;
			}
			// palette data now
			ushort[] paletteData = ConvertPalette(pi);
			byte[] processedData = new byte[pi.pixelIndexes.Count];
			for (int i = 0; i < pi.pixelIndexes.Count; ++i)
			{
				processedData[i] = (byte)(pi.pixelIndexes[i]);
			}
			if (paletteData.Length < 256)
			{
				Array.Resize(ref paletteData, 256);
			}
			// write data now
			FileStream fs = File.Open(fileName.Substring(0, fileName.Length - 3) + "sdi", FileMode.Create);
			fs.WriteByte(4);
			fs.WriteByte((byte)width);
			fs.WriteByte((byte)height);
			// whether or not it remains in ram
			// whether or not it remains in ram
			fs.WriteByte(retain ? (byte)1 : (byte)0);
			//  reference count
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// UV types
			fs.WriteByte((byte)UType);
			fs.WriteByte((byte)VType);
			// padding
			fs.WriteByte((byte)0);
			fs.WriteByte((byte)0);

			// palette pointer
			fs.Write(BitConverter.GetBytes((int)0x30));
			// image data pointer
			fs.Write(BitConverter.GetBytes(0x30 + (paletteData.Length * 2)));
			// char *texturename
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// previous (0x10)
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// next (0x14)
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// int texture id, int palette id
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// padding
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// palette now
			for (int i = 0; i < paletteData.Length; ++i)
			{
				fs.Write(BitConverter.GetBytes(paletteData[i]), 0, 2);
			}
			// image data now
			fs.Write(processedData, 0, processedData.Length);
			fs.Close();
			return;
		}

		static void CreatePC4(string fileName, int UType, int VType, bool retain)
		{
			Bitmap fileToConvert;
			try
			{
				fileToConvert = (Bitmap)Bitmap.FromFile(fileName);
			}
			catch
			{
				Console.WriteLine("Couldn't open file " + fileName);
				return;
			}
			PaletteImage pi = PaletteifyImage(fileToConvert, 16);
			int width = 0;
			int height = 0;
			switch (pi.width)
			{
				case 8:
					width = 0;
					break;
				case 16:
					width = 1;
					break;
				case 32:
					width = 2;
					break;
				case 64:
					width = 3;
					break;
				case 128:
					width = 4;
					break;
				case 256:
					width = 5;
					break;
				case 512:
					width = 6;
					break;
				case 1024:
					width = 7;
					break;
				default:
					Console.WriteLine("Image dimensions must be power of 2!");
					return;
					break;
			}
			switch (pi.height)
			{
				case 8:
					height = 0;
					break;
				case 16:
					height = 1;
					break;
				case 32:
					height = 2;
					break;
				case 64:
					height = 3;
					break;
				case 128:
					height = 4;
					break;
				case 256:
					height = 5;
					break;
				case 512:
					height = 6;
					break;
				case 1024:
					height = 7;
					break;
				default:
					Console.WriteLine("Image dimensions must be power of 2!");
					return;
					break;
			}
			// palette data now
			ushort[] paletteData = ConvertPalette(pi);
			byte[] processedData = new byte[pi.pixelIndexes.Count / 2];
			for (int i = 0; i < pi.pixelIndexes.Count; ++i)
			{
				if (i % 2 == 0)
				{
					processedData[i / 2] = (byte)(pi.pixelIndexes[i]);
				} else
				{
					processedData[i / 2] |= (byte)(pi.pixelIndexes[i] << 4);
				}
			}
			if (paletteData.Length < 16)
			{
				Array.Resize(ref paletteData, 16);
			}
			// write data now
			FileStream fs = File.Open(fileName.Substring(0, fileName.Length - 3) + "sdi", FileMode.Create);
			fs.WriteByte(3);
			fs.WriteByte((byte)width);
			fs.WriteByte((byte)height);
			// whether or not it remains in ram
			fs.WriteByte(retain ? (byte)1 : (byte)0);
			//  reference count
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// UV types
			fs.WriteByte((byte)UType);
			fs.WriteByte((byte)VType);
			// padding
			fs.WriteByte((byte)0);
			fs.WriteByte((byte)0);

			// palette pointer
			fs.Write(BitConverter.GetBytes((int)0x30));
			// image data pointer
			fs.Write(BitConverter.GetBytes(0x30 + (paletteData.Length * 2)));
			// char *texturename
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// previous (0x10)
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// next (0x14)
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// int texture id, int palette id
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// padding
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			fs.Write(BitConverter.GetBytes(0), 0, 4);
			// palette now
			for (int i = 0; i < paletteData.Length; ++i)
			{
				fs.Write(BitConverter.GetBytes(paletteData[i]), 0, 2);
			}
			// image data now
			fs.Write(processedData, 0, processedData.Length);
			fs.Close();
			return;
		}

		static ushort[] ConvertPalette(PaletteImage pi)
		{
			ushort[] paletteData = new ushort[pi.palette.Count];
			for (int i = 0; i < pi.palette.Count; ++i)
			{
				// generate the actual palette color
				ushort currentColor = 0;
				if (pi.palette[i].A == 255)
				{
					currentColor = 32768;
				}
				currentColor |= (ushort)((pi.palette[i].B >> 3) << 10);
				currentColor |= (ushort)((pi.palette[i].G >> 3) << 5);
				currentColor |= (ushort)(pi.palette[i].R >> 3);
				paletteData[i] = currentColor;
			}
			// re-organize pixels if one is 0 alpha
			for (int i = 1; i < pi.palette.Count; ++i)
			{
				if (pi.palette[i].A == 0)
				{
					ushort[] newPaletteData = new ushort[pi.palette.Count];
					newPaletteData[0] = paletteData[i];
					int currPalettePos = 1;
					// remap the palette
					for (int j = 0; j < pi.palette.Count; ++j)
					{
						if (j == i)
						{
							continue;
						}
						newPaletteData[currPalettePos] = paletteData[j];
						currPalettePos += 1;
					}
					paletteData = newPaletteData;

					// remap the pixel indices now
					for (int j = 0; j < pi.pixelIndexes.Count; ++j)
					{
						if (pi.pixelIndexes[j] < i)
						{
							pi.pixelIndexes[j] += 1;
						} else if (pi.pixelIndexes[j] == i)
						{
							pi.pixelIndexes[j] = 0;
						}
						// nothing needs to be done if you're above the original index...
					}

					break;
				}
			}
			return paletteData;
		}

		static void CreateTiled8(string fileName)
		{
			Bitmap fileToConvert;
			try
			{
				fileToConvert = (Bitmap)Bitmap.FromFile(fileName);
			}
			catch
			{
				Console.WriteLine("Couldn't open file " + fileName);
				return;
			}
			PaletteImage pi = PaletteifyImage(fileToConvert, 256);
			if (pi.width % 8 != 0 || pi.height % 8 != 0)
			{
				Console.WriteLine("Tiled images must have a resolution that's a multiple of 8!");
				return;
			}
			// palette data now
			ushort[] paletteData = ConvertPalette(pi);
			byte[] processedData = new byte[pi.pixelIndexes.Count];
			for (int i = 0; i < pi.pixelIndexes.Count; ++i)
			{
				processedData[i] = (byte)(pi.pixelIndexes[i]);
			}
			if (paletteData.Length < 256)
			{
				Array.Resize(ref paletteData, 256);
			}
			// write data now
			FileStream fs = File.Open(fileName.Substring(0, fileName.Length - 3) + "sds", FileMode.Create);
			fs.WriteByte((byte)0);
			// uploaded, sub, padding
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.Write(BitConverter.GetBytes(pi.width), 0, 4);
			fs.Write(BitConverter.GetBytes(pi.height), 0, 4);
			// format...0 = 16 color 1 = 256 color 2 = 16 bit direct bmp
			fs.WriteByte(1);
			// padding
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// 0x24 header size + 0x100 * 2 palette
			fs.Write(BitConverter.GetBytes(0x220), 0, 4);
			// palette offset
			fs.Write(BitConverter.GetBytes(0x20), 0, 4);
			// internal: id
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// internal: native
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// palette now
			for (int i = 0; i < paletteData.Length; ++i)
			{
				fs.Write(BitConverter.GetBytes(paletteData[i]), 0, 2);
			}
			// tile the data now...
			byte[] tiledData = new byte[processedData.Length];
			int xpos = 0;
			int ypos = 0;
			for (int i = 0; i < pi.height; i += 8)
			{
				for (int j = 0; j < pi.width; j += 8)
				{
					for (int k = 0; k < 8; ++k)
					{
						for (int l = 0; l < 8; ++l)
						{
							tiledData[i * pi.width + ((j + k) * 8) + l] = processedData[(i + k) * pi.width + j + l];
							++xpos;
						}
						xpos -= 8;
						++ypos;
					}
					ypos -= 8;
					xpos += 8;
				}
				xpos -= pi.width;
				ypos += 8;
			}

			// image data now
			fs.Write(tiledData, 0, tiledData.Length);
			fs.Close();
			return;
		}

		static void CreateSprite4(string fileName)
		{
			Bitmap fileToConvert;
			try
			{
				fileToConvert = (Bitmap)Bitmap.FromFile(fileName);
			}
			catch
			{
				Console.WriteLine("Couldn't open file " + fileName);
				return;
			}
			PaletteImage pi = PaletteifyImage(fileToConvert, 16);
			int resolution = 0;
			if (pi.width == 8 && pi.height == 8)
			{
				resolution = 0;
			}
			else if (pi.width == 16 && pi.height == 16)
			{
				resolution = 1;
			}
			else if (pi.width == 32 && pi.height == 32)
			{
				resolution = 2;
			}
			else if (pi.width == 64 && pi.height == 64)
			{
				resolution = 3;
			}
			else if (pi.width == 16 && pi.height == 8)
			{
				resolution = 4;
			}
			else if (pi.width == 32 && pi.height == 8)
			{
				resolution = 5;
			}
			else if (pi.width == 32 && pi.height == 16)
			{
				resolution = 6;
			}
			else if (pi.width == 64 && pi.height == 32)
			{
				resolution = 7;
			}
			else if (pi.width == 8 && pi.height == 16)
			{
				resolution = 8;
			}
			else if (pi.width == 8 && pi.height == 32)
			{
				resolution = 9;
			}
			else if (pi.width == 16 && pi.height == 32)
			{
				resolution = 10;
			}
			else if (pi.width == 32 && pi.height == 64)
			{
				resolution = 11;
			}
			else
			{
				Console.WriteLine("Invalid sprite resolution");
			}
			// palette data now
			ushort[] paletteData = ConvertPalette(pi);
			byte[] processedData = new byte[pi.pixelIndexes.Count / 2];
			for (int i = 0; i < pi.pixelIndexes.Count; ++i)
			{
				if (i % 2 == 0)
				{
					processedData[i / 2] = (byte)(pi.pixelIndexes[i]);
				}
				else
				{
					processedData[i / 2] |= (byte)(pi.pixelIndexes[i] << 4);
				}
			}
			if (paletteData.Length < 16)
			{
				Array.Resize(ref paletteData, 16);
			}
			// write data now
			FileStream fs = File.Open(fileName.Substring(0, fileName.Length - 3) + "sds", FileMode.Create);
			fs.WriteByte((byte)resolution);
			// uploaded, sub, padding
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.Write(BitConverter.GetBytes(pi.width), 0, 4);
			fs.Write(BitConverter.GetBytes(pi.height), 0, 4);
			// format...0 = 16 color 1 = 256 color 2 = 16 bit direct bmp
			fs.WriteByte(0);
			// padding
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// 0x20 header size + 0x10 * 2 palette
			fs.Write(BitConverter.GetBytes(0x40), 0, 4);
			// palette offset
			fs.Write(BitConverter.GetBytes(0x20), 0, 4);
			// internal: id
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// internal: native
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// palette now
			for (int i = 0; i < paletteData.Length; ++i)
			{
				fs.Write(BitConverter.GetBytes(paletteData[i]), 0, 2);
			}

			// image data now
			fs.Write(processedData, 0, processedData.Length);
			fs.Close();
			return;
		}

		static void CreateSprite8(string fileName)
		{
			Bitmap fileToConvert;
			try
			{
				fileToConvert = (Bitmap)Bitmap.FromFile(fileName);
			}
			catch
			{
				Console.WriteLine("Couldn't open file " + fileName);
				return;
			}
			PaletteImage pi = PaletteifyImage(fileToConvert, 256);
			int resolution = 0;
			if (pi.width == 8 && pi.height == 8)
			{
				resolution = 0;
			} else if (pi.width == 16 && pi.height == 16)
			{
				resolution = 1;
			} else if (pi.width == 32 && pi.height == 32)
			{
				resolution = 2;
			} else if (pi.width == 64 && pi.height == 64)
			{
				resolution = 3;
			} else if (pi.width == 16 && pi.height == 8)
			{
				resolution = 4;
			} else if (pi.width == 32 && pi.height == 8)
			{
				resolution = 5;
			} else if (pi.width == 32 && pi.height == 16)
			{
				resolution = 6;
			} else if (pi.width == 64 && pi.height == 32)
			{
				resolution = 7;
			} else if (pi.width == 8 && pi.height == 16)
			{
				resolution = 8;
			} else if (pi.width == 8 && pi.height == 32)
			{
				resolution = 9;
			} else if (pi.width == 16 && pi.height == 32)
			{
				resolution = 10;
			} else if (pi.width == 32 && pi.height == 64)
			{
				resolution = 11;
			} else
			{
				Console.WriteLine("Invalid sprite resolution");
			}
			// palette data now
			ushort[] paletteData = ConvertPalette(pi);
			byte[] processedData = new byte[pi.pixelIndexes.Count];
			for (int i = 0; i < pi.pixelIndexes.Count; ++i)
			{
				processedData[i] = (byte)(pi.pixelIndexes[i]);
			}
			if (paletteData.Length < 256)
			{
				Array.Resize(ref paletteData, 256);
			}
			// write data now
			FileStream fs = File.Open(fileName.Substring(0, fileName.Length - 3) + "sds", FileMode.Create);
			fs.WriteByte((byte)resolution);
			// uploaded, sub, padding
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.Write(BitConverter.GetBytes(pi.width), 0, 4);
			fs.Write(BitConverter.GetBytes(pi.height), 0, 4);
			// format...0 = 16 color 1 = 256 color 2 = 16 bit direct bmp
			fs.WriteByte(1);
			// padding
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// 0x20 header size + 0x100 * 2 palette
			fs.Write(BitConverter.GetBytes(0x220), 0, 4);
			// palette offset
			fs.Write(BitConverter.GetBytes(0x20), 0, 4);
			// internal: id
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// internal: native
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// palette now
			for (int i = 0; i < paletteData.Length; ++i)
			{
				fs.Write(BitConverter.GetBytes(paletteData[i]), 0, 2);
			}

			// image data now
			fs.Write(processedData, 0, processedData.Length);
			fs.Close();
			return;
		}

		static void CreateSprite16(string fileName)
		{
			Bitmap fileToConvert;
			try
			{
				fileToConvert = (Bitmap)Bitmap.FromFile(fileName);
			}
			catch
			{
				Console.WriteLine("Couldn't open file " + fileName);
				return;
			}
			int resolution = 0;
			if (fileToConvert.Width == 8 && fileToConvert.Height == 8)
			{
				resolution = 0;
			}
			else if (fileToConvert.Width == 16 && fileToConvert.Height == 16)
			{
				resolution = 1;
			}
			else if (fileToConvert.Width == 32 && fileToConvert.Height == 32)
			{
				resolution = 2;
			}
			else if (fileToConvert.Width == 64 && fileToConvert.Height == 64)
			{
				resolution = 3;
			}
			else if (fileToConvert.Width == 16 && fileToConvert.Height == 8)
			{
				resolution = 4;
			}
			else if (fileToConvert.Width == 32 && fileToConvert.Height == 8)
			{
				resolution = 5;
			}
			else if (fileToConvert.Width == 32 && fileToConvert.Height == 16)
			{
				resolution = 6;
			}
			else if (fileToConvert.Width == 64 && fileToConvert.Height == 32)
			{
				resolution = 7;
			}
			else if (fileToConvert.Width == 8 && fileToConvert.Height == 16)
			{
				resolution = 8;
			}
			else if (fileToConvert.Width == 8 && fileToConvert.Height == 32)
			{
				resolution = 9;
			}
			else if (fileToConvert.Width == 16 && fileToConvert.Height == 32)
			{
				resolution = 10;
			}
			else if (fileToConvert.Width == 32 && fileToConvert.Height == 64)
			{
				resolution = 11;
			}
			else
			{
				Console.WriteLine("Invalid sprite resolution");
			}
			ushort[] colorData = new ushort[fileToConvert.Width * fileToConvert.Height];
			for (int i = 0; i < fileToConvert.Height; ++i)
			{
				for (int i2 = 0; i2 < fileToConvert.Width; ++i2)
				{
					Color currPixel = fileToConvert.GetPixel(i2, i);
					ushort currentColor = 0;
					if (currPixel.A >= 127)
					{
						currentColor = 32768;
					}
					currentColor |= (ushort)((currPixel.B >> 3) << 10);
					currentColor |= (ushort)((currPixel.G >> 3) << 5);
					currentColor |= (ushort)(currPixel.R >> 3);
					colorData[(i * fileToConvert.Width) + i2] = currentColor;
				}
			}
			// write data now
			FileStream fs = File.Open(fileName.Substring(0, fileName.Length - 3) + "sds", FileMode.Create);
			fs.WriteByte((byte)resolution);
			// uploaded, sub, padding
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.Write(BitConverter.GetBytes(fileToConvert.Width), 0, 4);
			fs.Write(BitConverter.GetBytes(fileToConvert.Height), 0, 4);
			// format...0 = 16 color 1 = 256 color 2 = 16 bit direct bmp
			fs.WriteByte(2);
			// padding
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// 0x20 header size + 0x100 * 2 palette
			fs.Write(BitConverter.GetBytes(0x20), 0, 4);
			// palette offset
			fs.Write(BitConverter.GetBytes(0x0), 0, 4);
			// internal: id
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// internal: native
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			fs.WriteByte(0);
			// palette now

			// image data now
			for (int i = 0; i < colorData.Length; ++i)
			{
				fs.Write(BitConverter.GetBytes(colorData[i]), 0, 2);
			}
			fs.Close();
			return;
		}

		static void Main(string[] args)
		{
			if (args.Length < 2)
			{
				Console.WriteLine("Args:");
				Console.WriteLine("-pc4: 16 color palette");
				Console.WriteLine("-pc8: 256 color palette");
				Console.WriteLine("-pc16: direct 16 bit color");
				Console.WriteLine("-pc32: 32 color palette, 3 bit alpha");
				Console.WriteLine("-t8: 256 color palette, tiled format");
				Console.WriteLine("-s4: 16 color palette sprite");
				Console.WriteLine("-s8: 256 color palette sprite");
				Console.WriteLine("-s16: direct 16 bit color sprite");
				Console.WriteLine("-uc: Clamp X axis");
				Console.WriteLine("-um: Mirror X axis");
				Console.WriteLine("-vc: Clamp Y axis");
				Console.WriteLine("-vm: Mirror Y axis");
				Console.WriteLine("-retain: keep texture in memory after loading");
					return;
			}

			int utype = 0;
			int vtype = 0;
			bool retain = false;

			switch (args[1])
			{
				case "-uc":
					utype = 2;
					break;
				case "-um":
					utype = 1;
					break;
				case "-vc":
					vtype = 2;
					break;
				case "-vm":
					vtype = 1;
					break;
				case "-retain":
					retain = true;
					break;
			}

			if (args.Length > 2)
			{
				switch (args[2])
				{
					case "-uc":
						utype = 2;
						break;
					case "-um":
						utype = 1;
						break;
					case "-vc":
						vtype = 2;
						break;
					case "-vm":
						vtype = 1;
						break;
					case "-retain":
						retain = true;
						break;
				}
				if (args.Length > 3)
				{
					switch (args[3])
					{
						case "-uc":
							utype = 2;
							break;
						case "-um":
							utype = 1;
							break;
						case "-vc":
							vtype = 2;
							break;
						case "-vm":
							vtype = 1;
							break;
						case "-retain":
							retain = true;
							break;
					}
				}
			}


			switch (args[0])
			{
				case "-pc4":
					{
						CreatePC4(args[args.Length - 1], utype, vtype, retain);
					}
					break;
				case "-pc8":
					{
						CreatePC8(args[args.Length - 1], utype, vtype, retain);
					}
					break;
				case "-pc16":
					{
						CreatePC16(args[args.Length - 1], utype, vtype, retain);
					}
					break;
				case "-pc32":
					{
						CreatePC32(args[args.Length - 1], utype, vtype, retain);
					}
					break;
				case "-t8":
					{
						CreateTiled8(args[args.Length - 1]);
					}
					break;
				case "-s4":
					{
						CreateSprite4(args[args.Length - 1]);
					}
					break;
				case "-s8":
					{
						CreateSprite8(args[args.Length - 1]);
					}
					break;
				case "-s16":
					{
						CreateSprite16(args[args.Length - 1]);
					}
					break;
			}
		}
	}
}
