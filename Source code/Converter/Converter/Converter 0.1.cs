using System.DirectoryServices;
using System.Linq.Expressions;
using System.Net.NetworkInformation;
using System.Text;

namespace Converter
{
    public partial class Converter : Form
    {
        public Converter()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            try
            {
                Ping ping = new Ping();
                PingReply reply = ping.Send("apac.bosch.com");
            }
            catch
            {
                string userInput = Microsoft.VisualBasic.Interaction.InputBox("Enter NTID:", "Input NTID", "");
                SearchResult result = SearchUserInActiveDirectory(userInput);
                if (result != null)
                {
                    this.Show();
                }
                else
                {
                    MessageBox.Show("User not found in Active Directory.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    this.Close();
                }
            }




        }
        private SearchResult SearchUserInActiveDirectory(string username)
        {
            using (DirectorySearcher searcher = new DirectorySearcher())
            {
                searcher.Filter = $"(&(objectClass=user)(|(sAMAccountName={username})(displayName={username})))";

                try
                {
                    SearchResult result = searcher.FindOne();
                    return result;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error searching in Active Directory: {ex.Message}");
                    return null;
                }
            }
        }
        private void ComboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            button2.Text = "TO " + comboBox1.Text;
        }
        private void ComboBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            button1.Text = "TO " + comboBox2.Text;
        }

        private void button1_Click(object sender, EventArgs e)
        {
            if (comboBox2.SelectedIndex == 0)
            {
                string[] lines = richTextBox1.Lines; // Get an array of lines
                StringBuilder resultBuilder = new StringBuilder();

                foreach (string line in lines)
                {
                    string hex = line.Replace(" ", ""); // Remove spaces from the input line
                    byte[] bytes = new byte[hex.Length / 2];

                    for (int i = 0; i < hex.Length; i += 2)
                    {
                        bytes[i / 2] = Convert.ToByte(hex.Substring(i, 2), 16);
                    }

                    string asciiLine = Encoding.ASCII.GetString(bytes);
                    resultBuilder.AppendLine(asciiLine);
                }

                richTextBox2.Text = resultBuilder.ToString();
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            if (comboBox1.SelectedIndex == 0)
            {
                string[] lines = richTextBox2.Lines; // Get an array of lines
                StringBuilder resultBuilder = new StringBuilder();

                foreach (string line in lines)
                {
                    byte[] asciiBytes = Encoding.ASCII.GetBytes(line); // Convert ASCII to bytes
                    string hexLine = BitConverter.ToString(asciiBytes).Replace("-", " ");

                    resultBuilder.AppendLine(hexLine);
                }

                richTextBox1.Text = resultBuilder.ToString();
            }
        }
    }
}
