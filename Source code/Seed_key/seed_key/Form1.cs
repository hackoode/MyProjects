using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.DirectoryServices;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace seed_key
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            string searchName = textBox1.Text;
            SearchResult result = SearchUserInActiveDirectory(searchName);

            if (result != null)
            {
                this.Hide(); // Hide the current form
                Seed_key form = new Seed_key();
                form.ShowDialog();
                this.Close(); // Close the current form after the new form is closed

            }
            else
            {
                MessageBox.Show("User not found in Active Directory.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
    }

}
