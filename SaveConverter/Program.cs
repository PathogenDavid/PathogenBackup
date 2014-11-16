using System;
using System.IO;
using System.Diagnostics;

namespace SaveConverter
{
    /// <summary>
    /// It isn't entirely clear why, but my counterfeit cart of Zelda: Minish Cap saves data to the SRAM in a non-linear fasion.
    /// I know there is something weird with how EEPROM games save to the cart sometimes, so maybe I am seeing the opposite effect here.
    /// This program reorders the bytes to be linear.
    /// </summary>
    class Program
    {
        static void PrintUsage()
        {
            Console.WriteLine("Usage: SaveConverter in.sav out.sav");
        }

        const int shuffleChunkLength = 8;

        static void Main(string[] args)
        {
            // Validate command line arguments:
            if (args.Length != 2)
            {
                PrintUsage();
                return;
            }

            if (!File.Exists(args[0]))
            {
                Console.WriteLine("Input file doesn't exist!");
                Console.WriteLine();
                PrintUsage();
                return;
            }

            if (File.Exists(args[1]))
            {
                Console.WriteLine("Output file already exists!");
                Console.WriteLine();
                PrintUsage();
                return;
            }

            // Convert the file:
            using (FileStream inFile = new FileStream(args[0], FileMode.Open, FileAccess.Read))
            using (FileStream outFile = new FileStream(args[1], FileMode.Create, FileAccess.Write))
            {
                byte[] buf1 = new byte[shuffleChunkLength];
                byte[] buf2 = new byte[shuffleChunkLength];

                if ((inFile.Length % shuffleChunkLength) != 0)
                {
                    Console.WriteLine("Input file's length needs to be a multiple of {0} bytes!", shuffleChunkLength);
                    return;
                }

                while (inFile.Position != inFile.Length)
                {
                    inFile.Read(buf1, 0, shuffleChunkLength);

                    for (int i = 0; i < shuffleChunkLength; i++)
                    {
                        buf2[shuffleChunkLength - i - 1] = buf1[i];
                    }

                    outFile.Write(buf2, 0, shuffleChunkLength);
                }
            }

            Console.WriteLine("Done.");
            if (Debugger.IsAttached) { Console.ReadLine(); }
        }
    }
}
